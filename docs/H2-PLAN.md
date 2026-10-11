# horde 2 — plan of record

**Last verified: 2026-10-09**, against ROADMAP.md at `e8441e8` (the records commit for #1009),
which sits on main `b2177c2`, and DECISIONS.md through ADR-199 (`49e3e6f`). ROADMAP B387. This
refresh covers everything since the 2026-10-01 verification: rows B403–B451, ADR-186 to ADR-199
with their amendments, and the parts the B451 manual scaffold found without an id. The previous
verification (2026-10-01, records PR #888, `ecbb205`) is in git history.

This is the README-level orientation for horde 2: every part, the rows and ADRs that govern it,
its status, what it needs first, the order of work, and what 1.0 is. It is a MAP of the records,
not a second copy of them.

- **ROADMAP.md outranks this file.** Task state, acceptance criteria and rulings live in the rows
  cited here. Where this plan and ROADMAP disagree, ROADMAP wins, and the disagreement belongs in
  [Inconsistencies found](#inconsistencies-found) until the lead fixes one or the other.
- **The visual map** is [`docs/design/h2-plan-map.html`](design/h2-plan-map.html). Serve the repo
  with `python3 tools/serve_labs.py` and open it. Click a part to see its rows (read live from
  ROADMAP.md), its decisions, and what it unblocks. `?check=1` runs its self-check; check 4
  compares the map with the parts table below, and check 5 proves the critical path.
- **The user manual** (`docs/manual/`, B451) cites the part ids below. `manual_scaffold_check`
  (in `./verify fast`) fails if a manual entry names an id this table does not hold, so a part id
  is renamed or retired only together with the manual.

## How to read a status

| Status | Means |
|---|---|
| built | Exists and passes its gates; nothing more is owed for 1.0 except integration elsewhere. |
| in progress | Work is dispatched, or a built artefact (usually a lab) is waiting on the human's decisions. |
| ruled | The human has ruled what it is; no build is dispatched. |
| planned | Roadmapped, not yet ruled in detail and not started. |
| post-1.0 | After the stability line, by the human's ruling. |
| parked | Archived or tabled by the human. |

The table's last column is the part's place in 1.0 under the roster ADR-190 froze on 2026-10-07:
**in**, **conditional** (ships only if it meets the module 1.0 bar, B439, by the module cutoff),
**open** (not ruled), **out** (after 1.0) or **parked**.

No part of horde 2 is **built** yet in this sense. The shell does not exist, so nothing is
integrated. Everything that exists is a lab, a lifted or ported core, a sibling module or a tool.

## The human's summary (2026-09-30), and where each item lives

| The human said | Part | Governing |
|---|---|---|
| "Scalpel and Horde Legacy engines combined into the new primary engine for Horde." | `engine`, `port` | B327, B385 |
| "Bend laws system ported over from H1 (particularly the inertial mass-spring; my own proprietary concept)." | `bend` | B397, ADR-096, B278, B57 |
| "… pitch quantization interface from H1 will be ported over and made more generally available to other FX and devices …" | `quant`, `arps`, `seqgen` | B391, B88, ADR-190 A5 |
| "All relevant parameters will be made available to modulation." | `seams`, `modmatrix` | B275 (the manifest declares every parameter's mod limits) |
| "Modulators labs + envelopes labs will deliver the mod page interface." | `modulators` | B208, B226, B370, B377 |
| "All relevant parameters will be made available to morph." | `seams`, `morph` | B275 (the manifest declares every parameter's morph class) |
| "Morph page labs will deliver the morph." | `morph` | B211, B269, B424 |
| "Eventual inclusion of Tonality …" | `tonality`, `microtuning`, `mts` | B390, B431 |
| "History tree page concept will be ported over from H1 and rigorously tested for edge cases …" | `history` | B389 |
| "All FX modules will be designed and built. Their preset systems will be roped into the larger preset system." | `rack`, `maw`, `sluice`, `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `ott`, `kchorus`, `presets` | B393, ADR-190 A7–A8, B450, B395 |
| "The morph behavior of FX signal chains will be determined from labs." | `fxmorph` | B265, ADR-193, B394 |
| "Global presets will be designed to encompass all capabilities of the synth …" | `presets` | B395, B257, B381 |

The lead's additions, agreed by the human: the new Sub (`sub`), filters (`filters`), the
mixer/routing lab (`mix`), GUI 3 and the screens (`gui3`), the horde 2 shell (`shell`), the legacy
freeze tag (`freeze`), performance (`perf`), the Sluice dependency (`sluice`), the testing
apparatus (`testing`), and the B331 lab-sync order ([Order of work](#order-of-work)).

Added since by ruling (2026-10-02 to 2026-10-09): the noise oscillator (`noise`), true stereo
(`stereo`), the master strip and limiter (`master`), MTS-ESP (`mts`), ECHO (`echo`), Bulwark
(`bulwark`), the three simple FX modules (`eq`, `fxfilter`, `drive`), Blind-Spot Armor (`armor`),
conformance and distribution (`distrib`) and the user manual (`manual`).

## The parts

"Needs first" lists completion prerequisites: a part cannot be done before them. In the map, an
edge that cites a row or ADR is solid, and an edge that is this plan's own inference is dashed.
The basis of every edge is in the map (hover an edge, or click a part).

**Changes in this refresh.** 15 parts added (`noise` re-scoped in place counts as a change, not
an addition): `mts`, `stereo`, `master`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `armor`,
`distrib`, `manual`, `sampler`, `seqgen`, `vintage`, `standalone`. One retired: `fxsimple`, split
into `eq`, `fxfilter` and `drive` by ADR-190 A7. Re-scoped in place, ids kept: `noise` (was the
post-1.0 noise osc / sampler; now the 1.0 noise oscillator, the sampler is `sampler`), `arps`
(was all arps, sequencers and generators, 1.0 open; now the 1.0 simple arpeggiator, the rest is
`seqgen`), `ott` (now Bulwark's multiband mode with the ATM preset, B423), `maw` (Shriek) and
`reverb` (Scape).

<!-- parts-table:start -->
| Part | id | Status | Governing rows | Needs first | 1.0 |
|---|---|---|---|---|---|
| Composed engine | `engine` | in progress | B327, B298, B252, B335, B310, B382, B355 | — | in |
| Engine audit + legacy roundup | `audit` | ruled | B376, B386, B416, B420, B312 | `engine` | in |
| Edge correction | `edges` | in progress | B383, B380, B378 | — | in |
| Clean C++ port (phase 1b) | `port` | in progress | B385, B405, B404, B406, B379, B332, B378 | `engine`, `edges` | in |
| Seam contracts + readiness gate | `seams` | ruled | B275, B276, B277, B339 | `audit` | in |
| Bend laws + glide | `bend` | planned | B397, B278, B57, B32 | `port`, `seams` | in |
| Shared pitch quantizer | `quant` | planned | B391, B88, B260 | `bend` | in |
| MTS-ESP client | `mts` | ruled | B431, B390 | `quant` | in |
| MPE, aftertouch + MIDI basics | `mpe` | planned | B388, B431, B429 | `bend`, `modmatrix`, `port` | in |
| Simple arpeggiator | `arps` | planned | B391, B431 | `quant` | in |
| Modulators + envelopes | `modulators` | in progress | B208, B226, B366, B370, B377, B253, B259, B264, B267, B126 | `engine` | in |
| Mod matrix | `modmatrix` | in progress | B392, B207, B57, B270, B282 | `modulators`, `seams`, `audit` | in |
| Modulator morph | `modmorph` | planned | B396 | `modmatrix`, `morph` | in |
| Morph: editor, laws, quantum | `morph` | in progress | B211, B235, B268, B269, B272, B424, B240, B308 | `seams` | in |
| Macros + intent bus | `intent` | ruled | B443, B442, B170, B270, B354 | `morph`, `modmatrix` | in |
| FX algorithm morph | `fxmorph` | in progress | B265, B394, B266, B352 | `morph`, `rack`, `sluice` | in |
| Patch model → state schema | `patch` | in progress | B263, B308, B258 | `shell`, `morph`, `modmatrix`, `mix` | in |
| History tree | `history` | planned | B389, B84, B119, B122, B186, B222, B193 | `shell`, `patch`, `fxmorph`, `intent` | in |
| The new Sub | `sub` | planned | B399, B327, B278, B282 | `seams`, `bend` | in |
| Noise oscillator | `noise` | planned | B409, B327 | `seams` | in |
| Filters | `filters` | in progress | B274, B287, B288, B289, B290, B303 | `seams`, `bend` | in |
| True stereo | `stereo` | planned | B408, B414 | `engine`, `filters` | in |
| Mixer + routing | `mix` | in progress | B402, B225, B435, B23, B258, B263 | `sub`, `noise`, `filters` | in |
| Master strip + limiter | `master` | ruled | B438, B402, B435 | `mix` | in |
| FX rack + slot contract | `rack` | planned | B450, B439, B50, B281, B262, B393, B435, B429 | `mix`, `seams` | in |
| Shriek (formerly MAW) | `maw` | in progress | B318, B373, B393, B433 | `rack` | in |
| Sluice | `sluice` | in progress | B328, B321, B329, B337, B347, B353, B361, B369, B436, B273, B359, B393 | `rack` | in |
| Scape (reverb) | `reverb` | in progress | B421, B152, B210, B393 | `rack` | in |
| ECHO (delay) | `echo` | planned | B425, B386 | `rack` | in |
| Bulwark compressor | `bulwark` | in progress | B421, B234, B423, B450 | `rack` | in |
| EQ | `eq` | planned | B234, B210, B336 | `rack` | in |
| FX filter | `fxfilter` | planned | B210, B288, B336 | `rack` | in |
| Drive | `drive` | planned | B210, B336 | `rack` | in |
| Bulwark multiband + ATM (was OTT) | `ott` | planned | B423, B400, B393 | `bulwark` | conditional |
| Kuramoto chorus | `kchorus` | planned | B401, B393 | `rack` | conditional |
| horde 2 plugin shell | `shell` | ruled | B398, B428, B429, B449, B305, B308, B306 | `seams`, `port` | in |
| Legacy freeze tag | `freeze` | ruled | B255, B308, B305 | — | in |
| Testing apparatus | `testing` | in progress | B316, B324, B340, B344, B350, B381, B186 | `port` | in |
| Performance | `perf` | in progress | B441, B445, B323, B372, B375, B357, B262 | `port`, `edges` | in |
| Blind-Spot Armor + security gates | `armor` | in progress | B448, B446, B430 | — | in |
| Conformance, distribution + legal | `distrib` | planned | B427, B447, B412, B433 | `shell` | in |
| GUI 3 + the screens | `gui3` | planned | B302, B449, B432, B271, B322, B336, B261, B308 | `shell`, `fxmorph`, `intent`, `modmorph`, `mix`, `maw`, `sluice` | in |
| Presets + factory library | `presets` | planned | B395, B257, B261, B312, B349 | `history`, `testing`, `intent`, `maw`, `sluice`, `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `gui3` | in |
| User manual | `manual` | in progress | B451, B413 | `gui3` | open |
| 1.0 — the stability line | `release` | planned | B305, B327, B440 | `presets`, `gui3`, `perf`, `freeze`, `quant`, `sub`, `noise`, `stereo`, `master`, `mpe`, `mts`, `arps`, `distrib`, `armor` | in |
| Tonality integration | `tonality` | post-1.0 | B390 | `release`, `quant` | out |
| Microtuning | `microtuning` | post-1.0 | B390 | `tonality` | out |
| Granular module | `granular` | post-1.0 | B393 | `release`, `rack` | out |
| Sampler | `sampler` | post-1.0 | B327, B330 | `release`, `noise` | out |
| Complex arps, step sequencers, generators | `seqgen` | post-1.0 | B391 | `release`, `arps` | out |
| Vintage-textures FX module | `vintage` | post-1.0 | B444 | `release`, `rack` | out |
| Standalone application | `standalone` | post-1.0 | B411 | `release` | out |
| Archived engines | `archived` | parked | B330, B363, B292 | — | parked |
<!-- parts-table:end -->

## The parts in detail

Each part: what it is · governing rows and ADRs · status · needs first · the human's open
decisions (collected again in [one list](#open-human-decisions)).

### Engine

**Composed engine** (`engine`). Two SCALPEL oscillators, each the SCALPEL blades on horde's
Kuramoto swarm: the "Scalpel and Horde Legacy engines combined" of the summary.
`docs/design/scalpel-horde-engine.js` is the golden the C++ is ported against (ADR-187 §3).
- Governing: B327 (the 1.0 lineup), B298, B252, B335, B310, B382 (M1–M3), B355 (ADR-189 D1–D3).
  ADR-184, ADR-187, ADR-189, ADR-191.
- Status: **in progress**. The lineup is ruled and the engine is built in the lab. Its parameter
  surface was decided in the audit. The lab default is os 1 since #984 (ADR-191, B445).
- Needs first: nothing.
- Open: envelope decision 1 (B366, B377); ADR-189's instrument defaults (ruled when the shell is
  built); whether the lab offers law 3 with a real bpm (B382).

**Engine audit and legacy roundup** (`audit`). Every control across the swarm core, the legacy
shell, the composed engine and SCALPEL, decided keep, lock, cull or merge. Then every Legacy
feature not yet in horde 2, approved or denied. The export is the provisional manifest
(ADR-186 §5(a)).
- Governing: B376, B386, B416 (the round-2 view), B420 (the deferred tests), B312. ADR-186.
- Status: **ruled**. Rounds 1 and 2 were decided by the human on 2026-10-02 (exports in
  `docs/audits/`), and the provisional manifest went to FOUNDATIONS (#104). The legacy roundup was
  ratified on 2026-10-03, settling all 156 items: for example ECHO (B425), stepped morph glide
  (B424), and a new horde 2 routing matrix instead of the legacy crosspoint (B402).
- Needs first: `engine`.
- Open: the 19 rows round 2 deferred, and B420's seven deferred tests.

**Edge correction** (`edges`). How edges are band-limited (B378 F3): the 2-point polyBLEP,
oversampling, linear- or minimum-phase BLEP tables, or note-dependent schemes.
- Governing: B383 (listening lab, merged #878), B380, B378. ADR-187, ADR-191.
- Status: **in progress**. The lab is built and the choice is by ear. With os 1 as the default
  (ADR-191), the edges carry more of the anti-aliasing.
- Needs first: nothing.
- Open: which option, (a)–(f).

**Clean C++ port, phase 1b** (`port`). One C++ composed engine in `h2/engine/`, ported fresh
against the composed JS, reusing proven code by copy and cleanup.
- Governing: B385, B405 (the parity wiring), B404 (the deferred quality suites), B406 (the
  wiring's follow-ups), B379, B332, B378. ADR-186 §4, ADR-187 (A2).
- Status: **in progress**. The engine is merged at parity (#889–#893) and its parity check is
  wired into `verify full` (#895): 540 of 543 scenarios, plus the 3 ring rows on a measured onset
  window (ADR-187 A2). Still owed: the aliasing and zipper suites (B404) and the B378 fixes as
  ledgered divergences. The CPU work is under `perf`.
- Needs first: `engine`; `edges` (F3 enters as a divergence once ruled; B379).
- Open: `verify full` now runs about 600 s, which needs a budget or a split into stages (B406
  item 4, a human gate); whether the floor's key-mismatch warning fails the gate (B406 item 5);
  the JS demotion ruling (ADR-187 §4) once the Layer-0 suite lands.

**Seam contracts and the readiness gate** (`seams`). An expectation contract per seam, a manifest
per engine, and a gate that refuses an incomplete manifest. This is the mechanism behind "all
relevant parameters will be made available to modulation" and "to morph".
- Governing: B275, B276 (done), B277 (not dispatched), B339 (ball: Sluice for (b), respond by
  2026-10-12). ADR-186 §5(a), §5(d).
- Status: **ruled**. B275 (b) is ruled: every source follows every global unless its manifest
  declares an opt-out with a reason.
- Needs first: `audit`.
- Open: B275 (a) the seam list and what "ready" means; (c) the FOUNDATIONS / horde split; (d)
  proceeding on the provisional schema before FOUNDATIONS answers.

### Pitch

**Bend laws and glide** (`bend`). H1's pitch system: GlideCore's five laws on the bend lane and
the note lane, including the inertial mass-spring (the human's own concept), per-note bend under
the same law, and Glide From's three sources.
- Governing: B397, B278, B57, B32. ADR-096, ADR-097, ADR-102, ADR-103, ADR-180.
- Status: **planned**. Built in legacy; B397 covers the port. The legacy roundup adds the
  simulated bend wheel and graphs after the bend laws (B386 item 8).
- Needs first: `port`; `seams`.

**The shared pitch quantizer** (`quant`). Legacy's scale quantize becomes a service other FX and
devices can call, with room for Tonality (B390). **In** for 1.0 (ADR-190 A5).
- Governing: B391, B88, B260. ADR-106, ADR-158, ADR-093, ADR-190. Status: **planned**.
- Needs first: `bend` (the quantizer is anchored on the bend lane, ADR-106).
- Open: whether a MIDI sidechain sets the scale in 1.0 (B260).

**MTS-ESP client** (`mts`). A basic MTS-ESP client (the 0BSD library, real-time safe), with a
stated precedence between horde's quantizer and scale and MTS-ESP. Ruled in on 2026-10-04 (B431):
"MTS-ESP microtuning in 1.0 is fine." It narrows B390: only this client comes forward.
- Governing: B431, B390. ADR-190. Status: **ruled**, not built. Needs first: `quant` (B431's
  precedence).

**MPE, aftertouch and MIDI basics** (`mpe`). Per-note pitch, pressure, timbre and slide reaching
the swarm and blades, the mod matrix, morph and the voice law; channel aftertouch as a mod
source; and the musician basics: the sustain pedal (CC64, ship-blocking), an init patch, MIDI
learn, mono/legato/portamento modes and per-patch bend range.
- Governing: B388, B431, B429. ADR-161, ADR-162, ADR-097, ADR-149, ADR-190.
- Status: **planned**. **In** for 1.0 (ADR-190 A4): MPE arrives through CLAP per-note
  expressions, not the wrapper's MIDI proxy parameters (B429 item 5).
- Needs first: `bend`, `modmatrix`, `port`.
- Open: the MPE design itself (B388).

**Simple arpeggiator** (`arps`). A simple arpeggiator consuming the shared quantizer. **In** for
1.0 (ADR-190 A5). Complex patterns, step sequencers and generators are `seqgen`, after 1.0.
- Governing: B391, B431. ADR-190. Status: **planned**. Needs first: `quant`.

### Modulation

**Modulators and envelopes** (`modulators`). The modulator lab (LFOs, the Kuro-LFO, ORBITAL, MIDI
trackers, note-on randoms, the dry-signal follower, coherence R as a source) and the envelope
hierarchy (ENV 1 global, shapeable stage curves). Together they deliver the mod page.
- Governing: B208, B226, B366, B370, B377, B253, B259, B264, B267, B126. ADR-165, ADR-161, ADR-162.
- Status: **in progress**. The labs are built. Envelope decision 1 is open.
- Needs first: `engine`.
- Open: linked note-on randoms (B264); ORBITAL velocity (B267); the onset-scatter recommendation
  (B370).

**The mod matrix** (`modmatrix`). Routes, depths, per-route polarity, mod-on-mod, per-corner
modulation ranges. The spring displacement (B57) joins as a source.
- Governing: B392 (a near-term priority), B207 (the lab, built), B57, B270, B282. ADR-168,
  ADR-141, ADR-136.
- Status: **in progress**. The lab is built and is next in the human's queue.
- Needs first: `modulators`; `seams` (B282); `audit` (inferred: the destinations are the audited
  parameter surface).

**Modulator morph** (`modmorph`). Modulator shapes and parameters morph continuously between
corners; mappings jump by quantum flip; a cycle detector rejects any intermediate state that
forms a modulation cycle.
- Governing: B396 (part of B392). ADR-104. Status: **planned** (to workshop).
- Needs first: `modmatrix`, `morph`.
- Open: a continuous morph law per modulator type, ORBITAL's bodies included.

### Morph and state

**Morph: editor, laws, quantum** (`morph`). The morph field's blend, quantum and stepped classes;
waypoints and custom curves; the waypoint law as a setting; a position-free draw for horde 2;
morph glide, and stepped morph glide (a quantum/blend hybrid, **in** for 1.0, ADR-190 A12).
- Governing: B211, B235, B268, B269, B272, B424, B240, B308 (H3). ADR-104, ADR-109, ADR-185,
  ADR-186 §5(g), ADR-190.
- Status: **in progress**. Editor lab rounds are built; stepped morph glide is designed in the
  morph editor lab and lands with the morph-glide port (B424).
- Needs first: `seams`.
- Open: B269's items (per-group cohesion, boundary editing in BLEND/STEPPED, exempt and locks);
  the counter-based draw ADR, owed before the first horde 2 patch is saved (B308 H3).

**Macros and the intent bus** (`intent`). Eight macro slots: four fixed intents (Tone, Space,
Time, Motion) that mean the same in every global preset, and four named slots that flip as units
across the morph grid (ADR-192). The intent bus lives on horde's global mod matrix and binds
nothing automatically; a module may offer up to eight ordered, labelled macros, with no role
vocabulary (ADR-169 A4) and binding curves `lin`, `exp`, `log` (ADR-169 A3).
- Governing: B443, B442, B170, B270, B354. ADR-192, ADR-152, ADR-176, ADR-169, ADR-188 A1.
- Status: **ruled** (was planned). It is built in legacy through phase 2; phase 3 and the routing
  half are owed.
- Needs first: `morph`, `modmatrix`.
- Open: the phrase list for slots 5–8; approving the SPEC-INTENT-BUS amendment, drafted with a lab
  (ADR-192).

**FX algorithm morph** (`fxmorph`). How an FX chain's order and topology morph between corners
while satisfying every condition, with an FX chain per corner (the human, 2026-10-05).
- Governing: B265, B394, B266, B352. ADR-193 (round-2 rulings D1–D8), ADR-195, ADR-125, ADR-188,
  ADR-175.
- Status: **in progress**. Lab rounds 2, 3 and 3b are built (#957, #980, #982): static nodes on
  a fixed ring, where only the cables move. The human no longer hears the jarring clicks, and
  likes the rack, matrix and panel layout (2026-10-08).
- Needs first: `morph`; `rack` (B265); `sluice` (ADR-188).
- Open: p and K by ear (ADR-193 D8), and the items B265 still lists.

**Patch model, becoming horde 2's state schema** (`patch`). One system for oscillator parts, FX
racks, mod banks, corners and globals. It lands as a schema version (ADR-186 §5(c)).
- Governing: B263, B308, B258. ADR-186. Status: **in progress**, ratification pending.
- Needs first: `shell`; `morph`, `modmatrix`, `mix`.
- Open: ratify the patch model.

**History tree** (`history`). H1's history, re-earned on horde 2: a shell-owned snapshot tree,
gesture-level nodes, fork on restore. It must survive preset changes, morphing, modulation,
mapping changes, signal-flow changes and FX module edits, with B222's defects as regression rows.
- Governing: B389 (high priority), B84, B119, B122, B186, B222, B193. ADR-160.
- Status: **planned**.
- Needs first, to be DONE: `shell`, `patch`, `fxmorph`, `intent`. The PORT can start as soon as
  horde 2 has state ([Order of work](#order-of-work) A3).
- Open: confirm that modulated values in motion are not history and mapping changes are (B389).

### Sources and routing

**The new Sub** (`sub`). A new sub oscillator, workshopped. The legacy Sub is not lifted as it is.
- Governing: B399 (the workshop, not started), B327, B278, B282. ADR-178, ADR-190.
- Status: **planned**. Needs first: `seams`; `bend` (one published per-voice pitch, B278).
- Open: what the human dislikes in the legacy Sub (B327, B399 ask this first).

**Noise oscillator** (`noise`). A plain noise oscillator, a 1.0 source separate from the
post-1.0 sampler (ADR-190 A6, answering B409's question).
- Governing: B409, B327. ADR-190. It has **no row of its own** and no design yet.
- Status: **planned**. Needs first: `seams` (it follows every global unless its manifest opts
  out, B275 (b)).

**Filters** (`filters`). Many types with a fidelity programme, serial or parallel placement with
the `+` inlet, Comb as a keytracked filter type, per-note filters for keytracking.
- Governing: B274, B287, B288, B289, B290, B303. ADR-153.
- Status: **in progress**. Self-oscillation is tabled (B292).
- Needs first: `seams`; `bend` (keytracking reads the published pitch, B278).
- Open: re-ruling the tolerances physics refuses (B287 → B290); the per-note type list after a
  Release cost measurement (B303).

**True stereo** (`stereo`). A width equation, measured in a stereo lab: which parameters one width
control drives (per-voice pan, per-channel filter and delay offsets, modulator phase, swarm and
blade offsets, M/S), with a mono-compatibility guarantee. **In** for 1.0 (ADR-190 A9).
- Governing: B408, B414 (the research report, 2026-10-02). ADR-190.
- Status: **planned**; the lab has not started. `width-lab.html` is prior art (B407).
- Needs first: `engine`, `filters` (B408's candidates are swarm, blade and filter offsets).
- Open: per voice or post-sum; which candidates belong in 1.0 (B408).

**Mixer and routing** (`mix`). Sources to filters to FX, and the mixer page with meters and
latching clip warnings at every stage. The legacy crosspoint matrix is not ported: a new routing
matrix is designed from legacy's requirements (B386 item 11). Every module carries the standard
Input and Output Gain pair for gain staging (B435).
- Governing: B402 (planned), B225 (the legacy-era lab, built), B435, B23, B258, B263. ADR-088,
  ADR-175.
- Status: **in progress** (B225's lab exists; B402 carries it into horde 2).
- Needs first: `sub`, `noise`, `filters` (inferred: the strips are the sources, the routing
  places the filters).
- Open: which of the 18 taps become full meters (B225); corner FX buses (B258).

**Master strip and limiter** (`master`). One instance of Bulwark's limiter face, never a rack
slot, the last stage before the output. The mixer page's master strip has limiter on/off,
Ceiling, Release, a gain-reduction meter, the clip latch, and a cut-only Volume.
- Governing: B438 (approved 2026-10-04, "master only for 1.0"), B402, B435. ADR-195 (Ceiling
  default −1.0 dBFS), ADR-190.
- Status: **ruled**. Bulwark mirrored the −1.0 default; the thread closed 2026-10-09.
- Needs first: `mix` (the strip folds into the B402 lab).

### FX

The roster, as ADR-190 froze it: Shriek (formerly MAW), Sluice, Scape (the reverb), ECHO,
Bulwark's compressor, EQ, the FX filter and a simplified drive module are **in**. Bulwark's
multiband mode with the ATM preset and the Kuramoto chorus are **conditional**. The vintage-textures
module and the granular module are **out**. Every module meets the module 1.0 bar (B439) by the
module cutoff (ADR-190 A1), or ships after 1.0.

**FX rack and the slot contract** (`rack`). What every module plugs into: one instance per module
type (ADR-172, ADR-193 D7), a slot contract for every type, cyclic topologies per sample
(ADR-175), the cost bench (B262).
- Governing: B450, B439, B50, B281, B262, B393, B435, B429. ADR-172, ADR-175, ADR-128, ADR-190,
  ADR-193, ADR-195.
- Status: **planned**. horde 2's rack is unbuilt. The hosted-module rack-slot contract is
  **RATIFIED** (`docs/proposals/rack-slot-contract.md`, ADR-201, 2026-10-09); Q3 and Q9 are
  provisional until measured. It takes FOUNDATIONS' FX-operator ABI as the code-level interface.
- Needs first: `mix` (inferred); `seams` (B281).
- Open: measure Q3 (the click ceiling) and Q9 (the state budget); build the admission test with
  the shell (B398); the module cutoff date.

**Shriek, formerly MAW** (`maw`). The three-stage saturator, FX-C, a hosted sibling module.
Renamed by the human on 2026-10-04 (B433); the part id stays `maw`. B318, B373, B393, B433.
ADR-170, ADR-092, ADR-190. **In progress.** Needs `rack`. Its level contract is answered (B435).
Open: FX-C as a fixed post-stage or a rack slot, and B318's other items.

**Sluice** (`sluice`). The morphable FX network, a sibling project. One patch per module XY;
different patches at horde's morph corners (ADR-188). B328, B321, B329, B337, B347, B353, B361,
B369, B436, B273, B359, B393. ADR-188, ADR-166, ADR-193. **In progress** (labs). Needs `rack`.
- Consumption is **ruled** on Sluice's side (Sluice D-096, recorded in B328 on 2026-10-04): horde
  takes Sluice's code into its own tree at Sluice V1, when Sluice goes public, with no tag and no
  lock file. The private-optional-dependency proposal is superseded.
- Open: the lab's tails (B369); ADR-193 D5 plans a per-module tail cap.

**Scape, the reverb** (`reverb`). Built as the sibling project Scape (B421, spun up 2026-10-02).
B421, B152, B210, B393. ADR-177, ADR-190, ADR-198. **In progress.** Needs `rack`.

**ECHO** (`echo`). horde's standard delay, built in horde, not a sibling (B425, ruled 2026-10-04):
a clean stereo delay with tempo sync, ping-pong, mid/side, feedback filtering and modulation.
B425, B386. ADR-190, ADR-142. **Planned.** Needs `rack`.

**Bulwark compressor** (`bulwark`). The compressor face of the sibling project Bulwark (formerly
Dynamite), hosted in the rack; its limiter face is `master`. B421, B234, B423, B450. ADR-190,
ADR-195, ADR-198. **In progress**: its M1–M3 are closed, and M4, hosting in horde, waits on the
slot contract (B450). Needs `rack`.

**EQ** (`eq`), **FX filter** (`fxfilter`) and **Drive** (`drive`). The three simple FX modules
(ADR-190 A7). EQ is the parametric EQ of FX lab round 2 (B234); comb and notch are filter types,
not modules (B288); drive is separate from Shriek so it can sit before and after another module.
B210, B234, B288, B336. ADR-190, ADR-172. **Planned**, each with **no row of its own**. Each
needs `rack`. Open: the names; ten glasses or six (B336).

**Bulwark multiband and ATM, was OTT** (`ott`). Not a module of its own: a multiband mode of
Bulwark's compressor, with ATM ("above the maximum") as a factory preset (the human, 2026-10-04,
B423). B423, B400, B393. ADR-190, ADR-195 (band count is flip-class, never summed in parallel with
an unphased branch). **Planned**, **conditional**. Needs `bulwark`.

**Kuramoto chorus** (`kchorus`). B401 (the module and its lab, not started), B393. ADR-190 A3.
**Planned**, **conditional**. Needs `rack`.

### Shell, quality and presentation

**horde 2 plugin shell** (`shell`). A separate product with all of its identity new (CLAP id
`com.mind-lathe.horde`, a new AU subtype, its own bundle, pkg identifier and preset root, the
vendor "Mindlathe", the display name "Horde"). Parameters are generated from manifests into a
committed lockfile; the state carries a schema version from day one.
- Governing: B398 (planned), B428 (frozen identities from the first release), B429 (process,
  tails and the wrapper boundary), B449 (R1–R11: MIDI input never stalled by GUI activity), B305,
  B308, B306. ADR-186, ADR-197 (a clean break from legacy saved state).
- Status: **ruled**. Nothing is built. B398's prerequisites, B385's parity and the B376/B386
  decisions, have landed.
- Needs first: `seams`; `port`.
- Open: which protected spec horde 2 answers to (B308 M2); the AU manufacturer code (ADR-186 §3).

**Legacy freeze tag** (`freeze`). The human cuts the legacy tag; CI archives the universal signed
bundles; old projects depend on that archive (ADR-186 §2). Legacy sessions stay on the old system
(ADR-197).
- Governing: B255, B308 (H1, H5, M4), B305. ADR-186. Status: **ruled**. No tag exists
  (checked 2026-10-09). Needs first: nothing in this graph.
- Open: cut the tag. (The real-blob corpus of B308 H5 is skipped: ADR-209 item 11.)

**Testing apparatus** (`testing`). The patch-space gauntlet, the blind listening passes, the
aliasing metric, the Serum 2 reference test and the C++ quality suites.
- Governing: B316, B324, B340, B344, B350, B381, B186. ADR-187. Status: **in progress**; Serum
  stage 1 merged (#886). Needs first: `port`.
- Open: loading Serum 2 after stage 1 (B381); the aliasDb re-ruling (B350).

**Performance** (`perf`). C++ CPU against the B439 budget (sources ≤ 34 % of a min-spec core for
8 voices; CPU is Layer-E, never a gate, ADR-187 §8), the voice limit, oversampling as a quality
option, a WASM build for the labs, R2 and R3, FX cost.
- Governing: B441 (the CPU campaign), B445 (os 1 default, os 2 HQ), B323, B372, B375, B357, B262.
  ADR-187, ADR-191.
- Status: **in progress**. Specialised kernels landed bit-identical (B441 phase 2), with the os 1
  default as the campaign's next lever; the flip is built (#984), with the ADR-197 migration.
- Needs first: `port`, `edges`.
- Open: the voice limit (B323); the WASM toolchain (B372); R2 and R3 (B357).

**Blind-Spot Armor and security gates** (`armor`). Every risk-register row is a named gate in
`./verify` or a visible hole on the dashboard; correctness and security share one catalogue.
Includes the runtime safety nets (B430).
- Governing: B448, B446, B430. ADR-197, ADR-194, ADR-196, ADR-199.
- Status: **in progress**. It has been law since 2026-10-08, and phase 1 is a retrofit, now. At
  ratification: 0 of 12 rows fully gated, 11 partial, 1 missing (soak).
- Needs first: nothing.

**Conformance, distribution and legal** (`distrib`). What a release must clear outside the DSP:
pluginval, auval and clap-validator on the signed builds (B427), the Windows host test before any
Windows release (B447), signed installers, licensing and the update path (B412), trademark and
licence items (B433). Platforms: macOS and Windows x64 (B432).
- Governing: B427, B447, B412, B433. ADR-190, ADR-196. Status: **planned**. Needs first: `shell`.
- Open: the commercial model, copy protection and update path (B412); the counsel items (B433).

**GUI 3 and the screens** (`gui3`). The labs assembled into a new GUI that binds only to
generated manifest keys (B308 L2); FX modules as software on screens; the settings page and file
manager. The GUI never interferes with playing; typed entry opens from double-clicking a value
readout, and double-clicking a knob resets it (B449); accessibility at Surge XT's level (B432).
- Governing: B302, B449, B432, B271, B322, B336, B261, B308. ADR-116, ADR-186, ADR-190.
- Status: **planned**. The screens workshops are built; the settings lab (B261) has not started.
- Needs first: `shell`, `fxmorph`, `intent`, `modmorph`, `mix`, `maw`, `sluice`.
- Open: one screen style for every FX module, or one each (B322).

**Presets and the factory library** (`presets`). Global presets that cover everything the synth
can do; saving one also ingests each of its corners; every FX module's presets roped in. The
preset format writes `os` explicitly or carries a format version (B395, ADR-197).
- Governing: B395, B257, B261, B312, B349. ADR-186, ADR-167, ADR-197. Status: **planned**.
- Needs first: `history`, `testing`, `intent`, `gui3`, and every 1.0 FX module (`maw`, `sluice`,
  `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`).
- Open: what "everything that fits under corner rule" means (B395); commissioning sound designers.

**User manual** (`manual`). The horde 1.0 manual in `docs/manual/`: 20 chapters, 244 sections and
a 233-entry figure registry, scaffolded 2026-10-09 (#1009). Writing the prose is not dispatched.
- Governing: B451, B413. ADR-186, ADR-190. Status: **in progress**. Needs first: `gui3` (its
  screenshots).
- Open: the lead's defaults, including "the manual is for 1.0", are open to the human (B451).

## Order of work

**PROPOSED by this plan. B331 still reads "Awaiting the human's reorder or approval."** ADR-190 A1
leans on it anyway: each module's scope list freezes at its lab sync (B331). On 2026-10-09 the lead
put horde at about S1–S2 (B450's response to Bulwark).

Two orders, on purpose. The [parts table](#the-parts)'s edges are COMPLETION dependencies. The
stages below order the human's REVIEW sessions, and a lab can rule its laws before its
prerequisites are built. The FX algorithm morph, for example, ruled D1–D8 and built three lab
rounds long before the rack exists.

**Track A — the C++ spine** (agent-paced; runs alongside the lab sessions):

| Step | Work | Parts |
|---|---|---|
| A1 | The clean port: parity is DONE and wired (B385, B405). Owed: the aliasing and zipper suites (B404), the B378 fixes and the edge correction as ledgered divergences, and the CPU campaign to the B439 budget (B441; the os 1 default landed, ADR-191). | `port`, `edges`, `testing`, `perf` |
| A2 | The shell skeleton (ADR-186 §5): the new identity; the manifest feeding a generated parameter lockfile (B308 H4), from B376's decided audit through B277 re-scoped to horde 2; the state schema version; the counter-based draw ADR (B308 H3). Frozen identities (B428), the wrapper boundary (B429) and the GUI rules R1–R11 (B449) land with it. | `seams`, `shell` |
| A3 | History on the skeleton (B389), ported as soon as horde 2 has state, its acceptance grown as each feature lands. | `history` |
| A4 | The pitch seam: the bend laws on one published per-voice pitch, the quantizer as a service, MTS-ESP with its stated precedence, and MPE through CLAP per-note expressions; room for Tonality (B390). | `bend`, `quant`, `mts`, `mpe` |
| A5 | The gates 1.0 must pass: the Blind-Spot Armor retrofit, now (ADR-197), and the conformance gates once a signed shell exists (B427). | `armor`, `distrib` |

**Track B — lab sync** (human-paced, one lab per review session; B331 reordered):

| Stage | Was in B331 | Work | Parts |
|---|---|---|---|
| S0 | S0 | Triage. Done in substance: the navigator archives the dead-engine and superseded labs (B407). | `archived` |
| S1 | S1 (part) | Engine decisions. B376 and B386 are decided (2026-10-02/03). Left: the SCALPEL interface (freezing its parameter set), the edge-correction listening (B383) and envelope decision 1. | `audit`, `engine`, `edges`, `modulators` |
| S2 | S2 (part) | **Modulation, promoted:** the modulators and envelopes lab, then the mod matrix lab (B392, with B57's spring source), then the modulator-morph workshop (B396). | `modulators`, `modmatrix`, `modmorph` |
| S3 | S2 (part) + S4 (head) | **Morph, with the FX algorithm morph promoted**: the morph editor with stepped morph glide (B424), the FX algorithm morph (ADR-193; rounds 2–3b built), the patch model (B263, the state schema), then the intent slots (ADR-192). | `morph`, `fxmorph`, `patch`, `intent` |
| S4 | S1 (part) + S3 | Sources, routing, stereo and performance input: the Sub workshop, the noise oscillator, filters, the stereo lab (B408), the mixer and routing with the master strip (B402, B438), the MPE design (B388) and the arpeggiator. | `sub`, `noise`, `filters`, `stereo`, `mix`, `master`, `mpe`, `arps` |
| S5 | S4 | FX modules and screens: the rack-slot contract first (B450, ratified, ADR-201), then the hosted siblings (Shriek, Sluice at V1, Scape, Bulwark) and horde's own (ECHO, EQ, FX filter, drive); the conditionals by the module cutoff; then the screen aesthetic and logos. | `rack`, `maw`, `sluice`, `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `ott`, `kchorus` |
| S6 | S5 | Presentation, then GUI 3. The settings lab (B261) comes first; B449's rules and B432's accessibility level apply. | `gui3` |
| S7 | S6 | Calibration and content: the Serum gauntlet (B381) and the listening pass, the random patch P4/P5, the legacy preset import (B312), the presets lab and factory bank (B395, B257), and the manual's prose and screenshots (B451). | `testing`, `presets`, `manual` |
| 1.0 | — | The legacy tag is cut and archived before the human moves their work (ADR-186 §2, §6). Modules that miss the cutoff ship after 1.0 (ADR-190 A1). Then the stability line. | `freeze`, `release` |

Why the Sub and filters sit in S4, not S1 where B331 put them: the clean port's parameter input
is the B376 audit, and before the stability line the manifest may grow (ADR-186 §5(e), (f)). So
the mod matrix and morph labs sync against the composed engine's parameters first. It is a
proposal for the human.

## Critical path to 1.0

The longest chain of completion prerequisites among the parts that are **in** for 1.0, computed by
the map from the table above. The map recomputes it on every load, and its self-check 5 proves
that no longer chain exists. As of this refresh, 12 parts, the same chain as on 2026-10-01:

`engine` → `audit` → `seams` → `bend` → `sub` → `mix` → `rack` → `sluice` → `fxmorph` →
`history` → `presets` → `release`

What it says:
- **The first links are mostly paid.** The audit is decided (`ruled`), and the port reached
  parity, so the C++ is no longer what the path waits on. The remaining head of the path is the
  seam gate (B275 (a), (c), (d), B277) and the bend-laws port (B397), neither dispatched.
- **The long pole is still sources → routing → rack.** The Sub workshop (B399) and the mixer lab
  (B402) have not started, though the rack-slot contract is now ratified (B450, ADR-201).
- **Sluice is on it.** The FX algorithm morph cannot finish without ADR-188's structural class for
  Sluice patches, and Sluice's code enters horde only at Sluice V1, when Sluice goes public (B328,
  Sluice D-096). That date is Sluice's, not horde's.
- **History is near the end of the path,** not because it starts late (A3), but because its
  acceptance lists every feature it must survive.
- **Two of its edges are inferences** (`sub` → `mix`, `mix` → `rack`), drawn dashed. If the human
  or the lead rejects one, the path changes.
- **Parts added in this refresh do not lengthen it.** The new 1.0 parts hang off shorter chains:
  `noise` and `stereo` feed `mix` or `release` directly, the new FX modules sit one step after
  `rack`, and `armor` has no prerequisite.
- **It is one of four tied chains of 12** (enumerated 2026-10-09): `filters` can stand in for
  `sub`, and `gui3` for `history`. The map highlights the first it finds, so shortening the path
  means shortening both alternatives.
- Parts that are conditional or open (`ott`, `kchorus`, `manual`) are left out of the computation.

## What 1.0 is

1.0 is **horde 2's first human-cut release tag: the stability line** (ADR-186 §5(f)). Before it,
laws, parameters and ranges change freely, and projects saved earlier may change sound. After it,
a sound-law change is selected by the state header's version, and parameter ids and normalized
ranges are locked by the lockfile.

**The roster froze on 2026-10-07** (ADR-190, with Amendments 1 and 2). Three dates, not one: the
roster locked at the freeze; each module's scope list freezes at its lab sync; the MUST rows of
the module 1.0 bar (B439) are judged at a later module cutoff, not yet set. A module that misses
the cutoff ships after 1.0, and anything undecided at the freeze went after 1.0 by default (A2).

What ships in it, by ruling (ADR-190 unless noted):
- **Sources:** the composed engine, two oscillators (B327, B385); the new Sub; the filters; the
  plain noise oscillator (A6).
- **FX:** Shriek; Sluice; Scape; ECHO; Bulwark's compressor; EQ, the FX filter and a simplified
  drive module (A7, A8).
- **Master:** Bulwark's master limiter on the master strip (B438).
- **Modulation and performance:** the modulators and envelopes, the mod matrix, modulator morph
  (B396), the macros with the intent bus (ADR-192), morph with stepped morph glide (A12) and the FX
  algorithm morph (B394), MPE plus channel aftertouch (A4), the quantizer and a simple arpeggiator
  (A5), MTS-ESP (B431), the bend laws (the human's summary, B397).
- **Platform:** true stereo (A9), history and undo (B389), macOS and Windows x64, and accessibility
  at the approved level (B432).
- **Presets and GUI:** the presets and factory library (B395) and GUI 3 (B302).
- **The shell:** the horde 2 shell with its own identity, as CLAP, VST3 and AU (ADR-186 §3).

**Conditional:** the Kuramoto chorus (A3), and Bulwark's multiband mode with the ATM preset
(B423). **Out:** see [Post-1.0](#post-10).

The gates it passes:
- `./verify full` green, which now includes the Blind-Spot Armor gates (ADR-197);
- the readiness gate on every source (B275);
- each core's ADR-187 status: parity-proven, or demoted with goldens and a Layer-0 suite;
- the module 1.0 bar on every module, judged at the cutoff (B439, ADR-190 A1);
- the conformance gates on the signed builds (B427), and the human's Windows host test before any
  Windows release (B447);
- the 55 shell-linked tools re-earned as horde 2's acceptance list (B308 H5).

Open for the human: the module cutoff date; whether the manual ships with 1.0 (B451).

## Post-1.0

| Part | Governing | Note |
|---|---|---|
| Tonality integration (`tonality`) | B390; docs/PARKED.md 13 | After 1.0 by ruling, but the seam room is required NOW: pitch as a seam input, not hard-coded 12-TET; modulation sources Tonality can feed later. |
| Microtuning (`microtuning`) | B390, B431 | horde's own microtuning waits for Tonality. The MTS-ESP client is in 1.0 (`mts`). |
| Granular module (`granular`) | B393, ADR-190 | Based on the granular sibling. |
| Sampler (`sampler`) | B327, B330, ADR-190 | The noise oscillator that doubles as a sampler. The plain noise oscillator is in 1.0. |
| Complex arps, step sequencers, generators (`seqgen`) | B391, ADR-190 A5 | "More complex patterns, step seq and generators after 1.0." |
| Vintage-textures FX module (`vintage`) | B444, ADR-190 Am. 2 | Tape warp and VCR artifacts; postponed 2026-10-07, nothing started. |
| Standalone application (`standalone`) | B411, ADR-190 A11 | The first post-1.0 deliverable. |

Also out of 1.0, and not parts of this graph: AAX (A10, B410), Windows on ARM, Linux and AUv3
(B432), and true-peak detection on the master (ADR-195).

## Parked

- **Archived engines** (`archived`): SPECTRA, STATION, CANTO and the swarmalator. They stay built
  and gated in the frozen legacy tree, and none is lifted (B330; docs/PARKED.md 19, 22, 23; ADR-182).
- **Tabled:** reverse FM (B363) and filter self-oscillation as an intentional option (B292).

## Open human decisions

One list, grouped by part. The row is where the answer is recorded. Renumbered in this refresh:
cite the row, not the number, because the numbers move.

Answered since 2026-10-01 and removed: the legacy roundup (B386, ratified 2026-10-03); the
bit-exact floor (ADR-187 A2); MPE in 1.0 (ADR-190 A4); the FX-morph I3 reading, STRICT or TAIL and
the approved set (ADR-193); Sluice's consumption mechanism and binary distribution (B328, Sluice
D-096); OTT and the Kuramoto chorus in 1.0 (ADR-190 A3, B423); arps and sequencers (ADR-190 A5);
a noise oscillator in 1.0 (ADR-190 A6).

1. **Order of work:** approve or reorder the lab-sync order (B331; this plan's proposal above).
2. **Engine audit:** the 19 rows round 2 deferred, and B420's deferred tests (B376, B420).
3. **Envelope decision 1:** which default curves, and what a time knob means (B366, B377).
4. **Edge correction:** option (a)–(f), by ear (B383, B380).
5. **ADR-189 defaults:** the D1–D3 defaults in the instrument (ruled when the shell is built).
6. **verify full runtime:** a budget or a split into staged gates; a `./verify` edit (B406 item 4).
7. **Floor warning:** should the floor's key-mismatch warning fail the gate (B406 item 5).
8. **JS demotion:** the composed engine's JS demoted, once its Layer-0 suite lands (ADR-187 §4).
9. **Seams:** B275 (a) the seam list and "ready", (c) the FOUNDATIONS / horde split, and (d)
   proceeding on the provisional schema.
10. **The legacy Sub:** what the human dislikes in it (B327, B399).
11. **Filters:** re-ruling the filter tolerances physics refuses (B287 → B290).
12. **Per-note filters:** the type list, after the Release cost measurement (B303).
13. **Scale sidechain:** whether a MIDI sidechain sets the scale in 1.0 (B260).
14. **MPE:** its design (B388).
15. **Note-on randoms:** the form of the linked pair (B264).
16. **ORBITAL velocity:** signed components or speed (B267).
17. **Onset scatter:** the recommendation (B370).
18. **Modulator morph:** a morph law per modulator type (B396).
19. **Morph editor:** B269's items: per-group cohesion, boundary editing, exempt and locks.
20. **Counter-based draws:** ratify the draw ADR before the first horde 2 patch is saved (B308 H3).
21. **Intent phrases:** the phrase list for the named slots 5–8 (ADR-192).
22. **Intent spec:** approve the SPEC-INTENT-BUS amendment for §3.1 and principle 2 (ADR-192).
23. **FX algorithm morph:** p and K by ear (ADR-193 D8), and the items B265 still lists.
24. **Patch model:** ratify it (B263).
25. **History scope:** confirm "modulated values in motion are not history; mapping changes are"
    (B389).
26. **True stereo:** per voice or post-sum, and which width candidates are in 1.0 (B408).
27. **Mixer taps:** which of the 18 become full meters (B225).
28. **Corner FX buses:** the discussion is owed (B258).
29. **Rack-slot contract:** ratify it and answer its 13 open questions (B450).
30. **Module cutoff:** the date the module 1.0 bar's MUST rows are judged (ADR-190 A1).
31. **Shriek:** FX-C as a fixed post-stage or a rack slot, and B318's other items (B318, B435).
32. **Sluice tails:** label, stand-in gate, or leave (B369).
33. **Module names:** ten glasses or six (B336).
34. **Screen styles:** one for every FX module, or one each (B322).
35. **Spec:** which protected spec horde 2 answers to (B308 M2).
36. **AU code:** the manufacturer code (ADR-186 §3; autonomous may rule a fleet code).
37. **Freeze tag:** cut it, and decide whether B308 H5's real-blob corpus must precede it. **Ruled 2026-10-10 (ADR-209 item 11): the corpus is skipped. Cutting the tag remains.**
38. **Voice limit:** a deterministic count or a documented CPU-adaptive mode (B323).
39. **WASM:** install the toolchain (B372).
40. **R2 and R3:** rule after the critic (B357).
41. **Serum 2:** load it after seeing stage 1 (B381).
42. **aliasDb:** the re-ruling (B350).
43. **Corner auto-ingest:** what "everything that fits under corner rule" means (B395).
44. **Sound designers:** whether to commission them for the 150–400-preset norm (B395).
45. **Commercial model:** sold, donation or open source; copy protection; signed installers; the
    update path (B412).
46. **Counsel items:** trademark screening and licences (B433).
47. **Manual:** the lead's defaults, including "the manual is for 1.0" (B451).

## Inconsistencies found

Flagged for the lead, not resolved here. Each carries its state: **RESOLVED** (fixed in the
records), **TRACKED** (owned by a row, not yet done) or **OPEN** (no row owns it). Items 1–12 are
from 2026-10-01, re-checked; items 13–22 are new in this refresh.

1. **RESOLVED.** Rows B385–B402 existed only on `lead-records-158`. PR #888 merged, and the map's
   check 1 reads every cited row.
2. **RESOLVED (ecbb205).** B379's status marker was stale; it now reads "STEP 1 MERGED (#876);
   SUPERSEDED by B385".
3. **RESOLVED (by B385/B405).** `h2/README.md` was stale against B385. It is now "Last verified:
   2026-10-01" with B405's wiring, and the stale phrases (the razor_core wait, "no divergences
   yet") are gone.
4. **TRACKED under B403, not yet done.** The charter's §Domain (CLAUDE.md) still predates ADR-186,
   B327 and B330: it presents SCALPEL as a CANDIDATE whose integration shape is "the open
   question" and CANTO as the first new member, and it still says MAW, renamed Shriek (B433).
5. **TRACKED under B403, not yet done.** B302 still frames GUI 3 as a switch-over from gui2, which
   belongs to the frozen legacy shell.
6. **TRACKED under B403, not yet done.** Legacy-tree rows B277, B278, B282, B288 and B289 are
   still not re-scoped per ADR-186's consequences.
7. **OPEN.** The B331 order is still "awaiting the human", while ADR-190 A1 already freezes scope
   lists "at its lab sync (B331)". The plan's order is a proposal, not a ruling.
8. **OPEN.** The freeze has different prerequisites in different records: ADR-186's note (after
   B255, which merged as #808), B308 ("H5's corpus and H1's tag come before the freeze") and
   ADR-186 §1(α). ADR-197's "legacy sessions can stay on the old system" does not settle it.
9. **TRACKED under B403, not yet done.** B263 carries no BUILT marker, though its lab is built.
10. **OPEN (unchanged).** The WASM build (one sentence in B372) and horde 2's voice limit (B323,
    B372, B375) still have no row of their own.
11. **RESOLVED (ADR-190).** The 1.0 FX list differed between B327 and B393. ADR-190 rules it.
12. **RESOLVED (ecbb205).** B391 deferred sequencing to the plan of record, which cannot rule.
13. **OPEN.** ROADMAP's head section "1.0 — DEFINITION OF DONE (RATIFIED 2026-09-10)" still sets
    out the legacy-era 1.0 (SWARM SAW, the legacy FX rack, 30–50 factory presets) and is not marked
    as superseded by ADR-186 and ADR-190. A reader of ROADMAP's head finds a different 1.0.
14. **OPEN.** B327 still says "PARKED post-1.0: a noise oscillator that doubles as a sampler" with
    no ADR-190 A6 note, and B409 is still "OPEN — for review" though A6 answered its question.
15. **OPEN.** B391 still says its 1.0 placement is the human's call; ADR-190 A5 answered it (a
    simple arpeggiator in; complex patterns, step sequencers and generators out).
16. **OPEN.** The FX roster rows lag ADR-190 and B423: B393 still reads "RULED — OTT lab and
    Kuramoto chorus to start", without A7/A8 or the Shriek rename; B400 (the OTT lab) is still
    "OPEN — planned", though B423 made OTT a multiband mode of Bulwark's compressor; B401 lacks
    A3's conditional status.
17. **OPEN.** Four 1.0 parts have no row of their own: the noise oscillator, EQ, the FX filter
    and the drive module (ADR-190 A6, A7). The arpeggiator shares B391 with the post-1.0
    sequencers and generators.
18. **OPEN.** Stale status markers. B405 reads "awaiting merge", but #895 merged (`2b8083c`).
    B416 reads "round 2 awaits the human", but B376 records round 2 decided on 2026-10-02. B385's
    marker still reads "OPEN — dispatched" after checkpoint 4 merged.
19. **OPEN.** B439 and B423 say a module that misses "the feature freeze" ships after 1.0, while
    ADR-190 A1 moved that judgment to the later module cutoff.
20. **OPEN.** B425 scopes ECHO "in the four-role face", which ADR-169 A4 retired.
21. **OPEN.** B435 cites "H2-PLAN item 26 / B318" for the FX-C ruling. This refresh renumbers
    the decision list (it is now item 31), so a ROADMAP citation by plan item number rots; cite
    B318.
22. **OPEN.** The B331 stage names no longer match the work: S0's triage is done in substance
    (B407), and S1's B376 and B386 decisions are made. B331's text was not updated.

## Keeping this true

- The lead updates this file when a ruling changes a part's status, rows or prerequisites, and
  updates the `DATA` block in `docs/design/h2-plan-map.html` in the same change. The map's
  self-check 4 fails if the two disagree.
- A part id is renamed or retired only together with `docs/manual/figures.json`;
  `manual_scaffold_check` fails otherwise.
- The map reads row titles and statuses live from ROADMAP.md. A part's status here is this plan's
  summary of those rows. When they move, the "Last verified" line above is what dates this
  summary.
