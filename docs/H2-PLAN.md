# horde 2 — plan of record

**Last verified: 2026-10-17**, every part, against ROADMAP.md and DECISIONS.md on main `3f338ce`
(the merge of records PR #1116): rows through B503, ADR-213 to ADR-231 with their amendments.
ROADMAP B387. This refresh covers everything since the partial refresh of 2026-10-11, above all
the engine rebuild the human ruled on 2026-10-15 (ADR-223). **Follow-up, later on 2026-10-17** (main `a94e97e` and the records after it, through B511 and ADR-236): B504 and ADR-232 now sit on main and are cited in the table and the map. The decisions the human answered that day (ADR-235, ADR-236) are marked in place, the freeze is built, and three new decisions are added (60–62).

The earlier verifications are in git history: 2026-10-09 (main `b2177c2`, through B451 and
ADR-199), its partial refresh of 2026-10-11 (main `6dd5884`, through B457 and ADR-212, ten parts),
and 2026-10-01 (records PR #888, `ecbb205`).

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
| superseded | Replaced by another part before it was done, by the human's ruling. The id is kept because the manual cites it; the part has no prerequisites and nothing depends on it. |

The table's last column is the part's place in 1.0 under the roster ADR-190 froze on 2026-10-07:
**in**, **conditional** (ships only if it meets the module 1.0 bar, B439, by the module cutoff),
**open** (not ruled), **out** (after 1.0), **parked**, or **superseded** (its place is taken by
the part that replaced it).

No part of horde 2 is **built** yet in this sense. The shell does not exist, so nothing is
integrated. Everything that exists is a lab, a lifted or ported core, a sibling module, a tool, the
new engine's spec and clean room, or the playable preview around the current engine (B458).

## The human's summary (2026-09-30), and where each item lives

| The human said | Part | Governing |
|---|---|---|
| "Scalpel and Horde Legacy engines combined into the new primary engine for Horde." | `engine2` (was `engine`, `port`) | B327, ADR-223, B494 |
| "Bend laws system ported over from H1 (particularly the inertial mass-spring; my own proprietary concept)." | `bend` | B397, ADR-096, B278, B57; B504 on PR #1117 |
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

Added since by ruling (2026-10-15): the new engine (`engine2`), rebuilt from a user-level spec in a
clean room (ADR-223, ADR-224, B494). It supersedes the composed engine (`engine`), the clean port
(`port`) and edge correction as its own part (`edges`, ruled by ADR-222 and carried into the new
spec's ED rows).

## The parts

"Needs first" lists completion prerequisites: a part cannot be done before them. In the map, an
edge that cites a row or ADR is solid, and an edge that is this plan's own inference is dashed.
The basis of every edge is in the map (hover an edge, or click a part).

**Changes in this refresh (2026-10-17).** One part added: `engine2`, the new engine (ADR-223,
B494), in progress. Three superseded, ids kept because `docs/manual/figures.json` cites `engine`
(33 entries) and `port` (19), and `manual_scaffold_check` refuses an id this table does not hold:
`engine` (ADR-223), `port` (ADR-223: a clean-room build, not a port) and `edges` (ruled option (d)
by ADR-222; its build B491 parked by ADR-223 item 5; carried into the spec's ED-1 to ED-6). The
manual itself is unchanged. `audit` stays **ruled**: it is the control audit and legacy roundup
(B376, B386), not the rebuild audit (B493, done), and its deferred items are still open as spec
Q31. Prerequisites re-pointed from the superseded parts to `engine2`: `modulators`, `bend`, `mpe`,
`stereo`, `shell`, `testing`, `perf`; `sub` and `filters` now need `engine2` instead of `bend`
(the published per-voice pitch B278 asked for is the engine's, ADR-225 item 7; the filter slot is
RC-1); `noise` gains `engine2`. Rows added: `mix` B499, B503; `rack` and `bulwark` B463; `shell`
B458; `testing` B490; `perf` B488; `distrib` B471, B472; `edges` B491; `engine` B458. The
2026-10-09 refresh's changes are in its trace (`traces/2026-10-09-h2-plan-refresh.md`).

<!-- parts-table:start -->
| Part | id | Status | Governing rows | Needs first | 1.0 |
|---|---|---|---|---|---|
| The new engine (clean room) | `engine2` | in progress | B494, B493, B497, B498, B500, B502 | `audit` | in |
| Composed engine | `engine` | superseded | B327, B298, B252, B335, B310, B382, B355, B458 | — | superseded |
| Engine audit + legacy roundup | `audit` | ruled | B376, B386, B416, B420, B312 | — | in |
| Edge correction | `edges` | superseded | B383, B380, B378, B491 | — | superseded |
| Clean C++ port (phase 1b) | `port` | superseded | B385, B405, B404, B406, B379, B332, B378 | — | superseded |
| Seam contracts + readiness gate | `seams` | ruled | B275, B276, B277, B339 | `audit` | in |
| Bend laws + glide | `bend` | planned | B397, B278, B57, B32, B504 | `engine2`, `seams` | in |
| Shared pitch quantizer | `quant` | planned | B391, B88, B260 | `bend` | in |
| MTS-ESP client | `mts` | ruled | B431, B390 | `quant` | in |
| MPE, aftertouch + MIDI basics | `mpe` | planned | B388, B431, B429 | `bend`, `modmatrix`, `engine2` | in |
| Simple arpeggiator | `arps` | planned | B391, B431 | `quant` | in |
| Modulators + envelopes | `modulators` | in progress | B208, B226, B366, B370, B377, B253, B259, B264, B267, B126 | `engine2` | in |
| Mod matrix | `modmatrix` | in progress | B392, B207, B57, B270, B282 | `modulators`, `seams`, `audit` | in |
| Modulator morph | `modmorph` | planned | B396 | `modmatrix`, `morph` | in |
| Morph: editor, laws, quantum | `morph` | in progress | B211, B235, B268, B269, B272, B424, B240, B308 | `seams` | in |
| Macros + intent bus | `intent` | ruled | B443, B442, B170, B270, B354, B453 | `morph`, `modmatrix` | in |
| FX algorithm morph | `fxmorph` | in progress | B265, B394, B266, B352 | `morph`, `rack`, `sluice` | in |
| Patch model → state schema | `patch` | in progress | B263, B308, B258 | `shell`, `morph`, `modmatrix`, `mix` | in |
| History tree | `history` | planned | B389, B84, B119, B122, B186, B222, B193 | `shell`, `patch`, `fxmorph`, `intent` | in |
| The new Sub | `sub` | planned | B399, B327, B278, B282 | `seams`, `engine2` | in |
| Noise oscillator | `noise` | planned | B409, B327 | `seams`, `engine2` | in |
| Filters | `filters` | in progress | B274, B287, B288, B289, B290, B303 | `seams`, `engine2` | in |
| True stereo | `stereo` | planned | B408, B414 | `engine2`, `filters` | in |
| Mixer + routing | `mix` | in progress | B402, B225, B435, B23, B258, B263, B499, B503 | `sub`, `noise`, `filters` | in |
| Master strip + limiter | `master` | ruled | B438, B402, B435 | `mix` | in |
| FX rack + slot contract | `rack` | planned | B450, B439, B50, B281, B262, B393, B435, B429, B463 | `mix`, `seams` | in |
| Shriek (formerly MAW) | `maw` | in progress | B318, B373, B393, B433 | `rack` | in |
| Sluice | `sluice` | in progress | B328, B321, B329, B337, B347, B353, B361, B369, B436, B273, B359, B393 | `rack` | in |
| Scape (reverb) | `reverb` | in progress | B421, B152, B210, B393 | `rack` | in |
| ECHO (delay) | `echo` | planned | B425, B386 | `rack` | in |
| Bulwark compressor | `bulwark` | in progress | B421, B234, B423, B450, B463 | `rack` | in |
| EQ | `eq` | planned | B234, B210, B336 | `rack` | in |
| FX filter | `fxfilter` | planned | B210, B288, B336 | `rack` | in |
| Drive | `drive` | planned | B210, B336 | `rack` | in |
| Bulwark multiband + ATM (was OTT) | `ott` | planned | B423, B400, B393 | `bulwark` | conditional |
| Kuramoto chorus | `kchorus` | planned | B401, B393 | `rack` | conditional |
| horde 2 plugin shell | `shell` | ruled | B398, B428, B429, B449, B305, B308, B306, B458 | `seams`, `engine2` | in |
| Legacy freeze tag | `freeze` | built | B255, B308, B305, B456 | — | in |
| Testing apparatus | `testing` | in progress | B316, B324, B340, B344, B350, B381, B186, B457, B490 | `engine2` | in |
| Performance | `perf` | in progress | B441, B445, B323, B372, B375, B357, B262, B488 | `engine2` | in |
| Blind-Spot Armor + security gates | `armor` | in progress | B448, B446, B430, B455, B457 | — | in |
| Conformance, distribution + legal | `distrib` | planned | B427, B447, B412, B433, B471, B472 | `shell` | in |
| GUI 3 + the screens | `gui3` | planned | B302, B449, B432, B271, B322, B336, B261, B308 | `shell`, `fxmorph`, `intent`, `modmorph`, `mix`, `maw`, `sluice` | in |
| Presets + factory library | `presets` | planned | B395, B257, B261, B312, B349, B453 | `history`, `testing`, `intent`, `maw`, `sluice`, `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `gui3` | in |
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

**The new engine** (`engine2`). horde 2's engine, rebuilt from a user-level spec
(`specs/SPEC-ENGINE.md`) in a clean room: one instrument-wide voice per played note holding both
swarm oscillators with their blades, the sub and the noise oscillator (ADR-224 item 1). The
instrument's laws are copied into the spec as normative math (ADR-224 A1), and eleven reserved
contracts leave room for what is still to come: the filters, the mixer's gain staging, the pitch
system, the modulator bank, the FX rack (RC-1 to RC-5, ADR-226 item 7), a source slot, host tempo
and transport, a GUI readout path, append-only parameters, a quality-tier hook (RC-6 to RC-10,
ADR-229) and history support (RC-11, ADR-229 A1). It is held to no JS parity: its correctness is
the ratified spec, its Layer-0 invariant suite and the human's ears on the bank (ADR-224 item 8).
The labs run it as WebAssembly; the JS twin is retired (ADR-223 item 4). This is the "Scalpel and
Horde Legacy engines combined" of the summary, now built once, cleanly ("ideally I only want to
have to do one more engine refresh", ADR-223).
- Governing: B494 (spec, tests, clean-room build, judgement), B493 (the rebuild audit), B497 (the
  clean-room tree and the test author's input data), B498 (the acceptance tests), B500 (the
  structure declaration for loupe's diagram), B502 (the two clean-room repositories). ADR-223 to
  ADR-231, ADR-222 (in the ED rows). ADR-232 is PROPOSED on PR #1117.
- Status: **in progress**. Done: the rebuild audit (B493, PR #1096: 35.4 % of today's engine is
  not about the sound); the spec, RATIFIED by the human's merge of PR #1097 on 2026-10-16 (eight
  rounds, two critic passes, five verification agents, two clean-room dry runs; 0 questions before
  the build, 25 during, 9 after) and amended since by ADR-230 and ADR-231; the 380 reference renders
  (B497, PR #1110, release asset `engine2-reference-renders-v1`); the structure declaration
  (B500, #1112); the two history-free repositories, `builder/` and `test-author/` (B502,
  2026-10-17). Next: the human opens the test-author session; the acceptance tests (B498, spec
  §10.1 items 0–23); then the build in BD-8's stages (manifest and state; the voice, ENV 1, the
  pedal and the output; the swarm; the edge correction; the blades; the sub and noise;
  modulation, morph and state; WebAssembly; then the bank). It is judged on the whole bank A/B, the
  CPU target (the heaviest patch at about 10.7 % of an M3 performance core per swarm, 8 voices;
  B488) and the invariant suite, and then replaces the composed engine (B494 (4)).
- Needs first: `audit` (BD-8 keeps the parameter table open until spec Q31, the audit's 19
  deferred rows, is ruled).
- Open: the spec's 25 questions ruled during the build and 9 after it (§11.1, §11.2); three gaps
  the structure declaration exposed (B500): where the oscillators return to the host rate, where
  IF-1's per-note volume and pan act, and who receives VO-10's lifecycle and IF-1's load readouts
  (ADR-232, proposed, names the pitch system as the lifecycle's consumer); ADR-189's D1–D3
  defaults in the instrument (the lead's reading: spec ED-1, BL-12 and C.14 may cover them; for
  the human to confirm); the WebAssembly toolchain's exact version (ADR-225 item 10).

**Engine audit and legacy roundup** (`audit`). Every control across the swarm core, the legacy
shell, the composed engine and SCALPEL, decided keep, lock, cull or merge. Then every Legacy
feature not yet in horde 2, approved or denied. The export is the provisional manifest
(ADR-186 §5(a)). Not to be confused with the rebuild audit of 2026-10-15 (B493), which is part of
`engine2`.
- Governing: B376, B386, B416 (the round-2 view), B420 (the deferred tests), B312. ADR-186.
- Status: **ruled**. Rounds 1 and 2 were decided by the human on 2026-10-02 (exports in
  `docs/audits/`), and the provisional manifest went to FOUNDATIONS (#104). The legacy roundup was
  ratified on 2026-10-03, settling all 156 items: for example ECHO (B425), stepped morph glide
  (B424), and a new horde 2 routing matrix instead of the legacy crosspoint (B402). Its open items
  now live in the new engine's spec: the 19 deferred rows and envelope decision 1's default curves
  are Q31, built as the spec proposes until ruled during the build (SPEC-ENGINE §11.1).
- Needs first: nothing in this graph (the composed engine it audited exists).
- Open: spec Q31 (the 19 deferred rows), and B420's seven deferred tests (still OPEN — planned).

**Composed engine** (`engine`), **superseded**. Two SCALPEL oscillators on horde's Kuramoto swarm,
with `docs/design/scalpel-horde-engine.js` as its golden. The human ruled it rebuilt from a
user-level spec, with parity to the JS no longer a goal (ADR-223, 2026-10-15). Until the new engine
replaces it, it is the engine that plays, under ADR-187 (ADR-223 item 6): the playable preview
wraps it (B458, ADR-215, built and installed 2026-10-12); its frame hold (D6) and steal fade (D7)
landed as ledgered divergences (ADR-216); its default output is about 12 dB lower (ADR-221); it may
change its sound for speed when the change is heard A/B (ADR-220); D9 and D10 are parked (ADR-223
item 5). Its open decisions moved: envelope decision 1 to spec Q31 and ADR-226 item 10;
ADR-189's D1–D3 defaults to `engine2`, still open; law 3's tempo grid is cut from 1.0 in Q31's
proposal. B327, B298, B252, B335, B310, B382, B355, B458.

**Edge correction** (`edges`), **superseded**. RULED 2026-10-14: option (d), a 32-tap
minimum-phase band-limited step table read with interpolation, with the research note's three
changes (ADR-222). Its build on the composed engine (B491, D10) is PARKED on a local branch
(ADR-223 item 5), and the ruling lives on as the new engine's ED rows (ED-1 to ED-6), built at
BD-8's stage 4. B383, B380, B378, B491.

**Clean C++ port, phase 1b** (`port`), **superseded**. One C++ composed engine in `h2/engine/`,
ported against the composed JS. ADR-223 replaces the port with a clean-room build. `h2/engine/`
stays at parity (#889–#893; wired into `verify full`, B405) and plays in the preview until
cutover; its deferred aliasing and zipper suites (B404) are taken over by the new engine's
invariant suite (spec §10.1, B498). The spec counts 27 files and about 14,370 lines to retire at
cutover (B494). Still open while its gates run: `verify full`'s runtime of about 600 s (B406
item 4, a `./verify` edit and so a human gate) and whether the floor's key-mismatch warning fails
the gate (B406 item 5). B385, B405, B404, B406, B379, B332, B378.

**Seam contracts and the readiness gate** (`seams`). An expectation contract per seam, a manifest
per engine, and a gate that refuses an incomplete manifest. This is the mechanism behind "all
relevant parameters will be made available to modulation" and "to morph".
- Governing: B275, B276 (done), B277 (not dispatched), B339 (ball: Sluice for (b)). ADR-186
  §5(a), §5(d); ADR-226 item 7, ADR-229.
- Status: **ruled**. B275 (b) is ruled: every source follows every global unless its manifest
  declares an opt-out with a reason. The new engine's side of the seams is now specified: a
  manifest giving each parameter's morph, rate and modulation classes (SPEC-ENGINE §5.1) and the
  eleven reserved contracts RC-1 to RC-11. No record re-rules B275 (a), (c) or (d) against them,
  and B277 is still written against the legacy code's tables (`kEngineBlocks`, `kParams`), so the
  part is not answered.
- Needs first: `audit`.
- Open: B275 (a) the seam list and what "ready" means (whether RC-1 to RC-11 answer it is for the
  lead and the human to say); (c) the FOUNDATIONS / horde split; (d) proceeding on the provisional
  schema before FOUNDATIONS answers.

### Pitch

**Bend laws and glide** (`bend`). H1's pitch system: GlideCore's five laws on the bend lane and
the note lane, including the inertial mass-spring (the human's own concept), per-note bend under
the same law, and Glide From's three sources.
- Governing: B397, B278, B57, B32. ADR-096, ADR-097, ADR-102, ADR-103, ADR-180, ADR-225,
  ADR-226. On PR #1117 (open): B504 and ADR-232.
- Status: **planned**. Built in legacy; B397 covers the port. The legacy roundup adds the
  simulated bend wheel and graphs after the bend laws (B386 item 8). The new engine keeps the
  pitch system upstream and reserves its contract (RC-3, ADR-226 item 7): it provides the per-note
  pitch input (IF-2), the published sounding pitch (ADR-225 item 7) and gravity's place, and never
  glides, bends or quantizes itself; glide and bend are spec Q6 and Q7, ruled during the build.
  On PR #1117, not yet on main: **B504**, the glide system in horde 2 (the human: "the glide
  system (particularly inertial spring mode with scale quantization) is a big part of what excites
  me about this synth"), carrying forward poly glide, the four travel laws with the spring, scale
  quantization as a modifier, Glide From and the chord case legacy left open, planned after the
  engine's stage 2; and **ADR-232**, which makes room for it in the engine (a pitch applies from a
  note's first sample; the pitch system receives the voice lifecycle).
- Needs first: `engine2` (RC-3); `seams`.
- Open: do the bank's 22 poly presets that store a glide time glide by default? Deferred by the
  human ("happy to save poly glide for another day"); on main it is B497's open item, and B504
  takes it over.

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
- Governing: B388, B431, B429. ADR-161, ADR-162, ADR-097, ADR-149, ADR-190, ADR-224, ADR-226.
- Status: **planned**. **In** for 1.0 (ADR-190 A4): MPE arrives through CLAP per-note
  expressions, not the wrapper's MIDI proxy parameters (B429 item 5). In the new engine voices are
  addressed by note id, so MPE works (ADR-224 item 1); the sustain pedal and legato are engine
  behaviour (ADR-226 items 1, 3); velocity and aftertouch are modulation sources mapped by default
  (ADR-226 item 4; aftertouch's destination is spec X21).
- Needs first: `bend`, `modmatrix`, `engine2`.
- Open: the MPE design (B388); which expressions reach the swarm and the blades is spec Q2.

**Simple arpeggiator** (`arps`). A simple arpeggiator consuming the shared quantizer. **In** for
1.0 (ADR-190 A5). Complex patterns, step sequencers and generators are `seqgen`, after 1.0.
- Governing: B391, B431. ADR-190. Status: **planned**. Needs first: `quant`.

### Modulation

**Modulators and envelopes** (`modulators`). The modulator lab (LFOs, the Kuro-LFO, ORBITAL, MIDI
trackers, note-on randoms, the dry-signal follower, coherence R as a source) and the envelope
hierarchy (ENV 1 global, shapeable stage curves). Together they deliver the mod page.
- Governing: B208, B226, B366, B370, B377, B253, B259, B264, B267, B126. ADR-165, ADR-161, ADR-162,
  ADR-224, ADR-226.
- Status: **in progress**. The labs are built. The new engine reserves the modulator bank's
  contract (RC-4): per-voice offsets by note id at a control rate of about 1,500 a second, ramped,
  and at audio rate on named destinations (ADR-224 items 4, 5; ADR-225 item 2). Envelope timing is
  ruled: a stage's time is exactly when it ends, under one law for ENV 1 and the bank's envelopes
  (ADR-226 item 10). The default curves are spec Q31.
- Needs first: `engine2` (RC-4).
- Open: linked note-on randoms (B264); ORBITAL velocity (B267); the onset-scatter recommendation
  (B370; where it lives is spec Q13).

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
- Governing: B389 (high priority), B84, B119, B122, B186, B222, B193. ADR-160, ADR-229.
- Status: **planned**. The new engine reserves what history needs (RC-11, ADR-229 A1): no hidden
  user state, a real-time-safe snapshot and restore, the host's gestures passed through so one
  drag is one undo step, and a restore that re-triggers no note. The undo stack itself is the
  shell's and the GUI's.
- Needs first, to be DONE: `shell`, `patch`, `fxmorph`, `intent`. The PORT can start as soon as
  horde 2 has state ([Order of work](#order-of-work) A3).
- Open: confirm that modulated values in motion are not history and mapping changes are (B389).

### Sources and routing

**The new Sub** (`sub`). A new sub oscillator, workshopped. The legacy Sub is not lifted as it is.
- Governing: B399 (the workshop, not started), B327, B278, B282. ADR-178, ADR-190, ADR-224,
  ADR-225.
- Status: **planned**. The new engine specifies a sub inside its voice (SPEC-ENGINE 5.5, the SN
  rows; built at BD-8 stage 6): it integrates its own phase from the voice's master phase, and
  ENV 1 acts after its tone filter, two departures from SPEC-SUBOSC §2 that ADR-225 item 6
  ratifies. The workshop designs what fills that place.
- Needs first: `seams`; `engine2` (the one published per-voice pitch B278 asked for is now the
  engine's, ADR-225 item 7; this edge replaces the earlier one to `bend`).
- Open: what the human dislikes in the legacy Sub (B327, B399 ask this first).

**Noise oscillator** (`noise`). A plain noise oscillator, a 1.0 source separate from the
post-1.0 sampler (ADR-190 A6, answering B409's question).
- Governing: B409, B327. ADR-190, ADR-224. It has **no row of its own** (B452 tracks it) and no
  design yet beyond the new engine's minimum (SPEC-ENGINE 5.6, SN-4; built at BD-8 stage 6).
- Status: **planned**. Needs first: `seams` (it follows every global unless its manifest opts
  out, B275 (b)); `engine2` (it is a source in the voice, ADR-224 item 1).
- Open: its design (spec X15, ruled during the build; its design lab, B452).

**Filters** (`filters`). Many types with a fidelity programme, serial or parallel placement with
the `+` inlet, Comb as a keytracked filter type, per-note filters for keytracking.
- Governing: B274, B287, B288, B289, B290, B303. ADR-153, ADR-226.
- Status: **in progress**. Self-oscillation is tabled (B292). The new engine reserves a per-voice
  stereo filter slot between the sources and ENV 1 (RC-1, ADR-226 item 7), handing it each
  source's signal, the sounding pitch for keytracking and the voice's lifecycle.
- Needs first: `seams`; `engine2` (the slot and the published pitch, RC-1; this edge replaces the
  earlier one to `bend`).
- Open: re-ruling the tolerances physics refuses (B287 → B290); the per-note type list after a
  Release cost measurement (B303); the filters' count and routing (spec X13, built until ruled as
  two per voice, serial or parallel).

**True stereo** (`stereo`). A width equation, measured in a stereo lab: which parameters one width
control drives (per-voice pan, per-channel filter and delay offsets, modulator phase, swarm and
blade offsets, M/S), with a mono-compatibility guarantee. **In** for 1.0 (ADR-190 A9).
- Governing: B408, B414 (the research report, 2026-10-02). ADR-190, ADR-224.
- Status: **planned**; the lab has not started. `width-lab.html` is prior art (B407). Its shape
  is ruled for the new engine (ADR-224 item 3, audit Q25): swarm members stay mono sources panned
  across the field, each voice is a stereo pair through the filter slot (RC-1), and true-stereo
  processing happens after the voice; no member is rendered per channel.
- Needs first: `engine2`, `filters` (B408's candidates are swarm, blade and filter offsets).
- Open: which width candidates belong in 1.0, and the mono-fold guarantee (B408; spec Q26, after
  the build).

**Mixer and routing** (`mix`). Sources to filters to FX, and the mixer page with meters and
latching clip warnings at every stage. The legacy crosspoint matrix is not ported: a new routing
matrix is designed from legacy's requirements (B386 item 11). Every module carries the standard
Input and Output Gain pair for gain staging (B435).
- Governing: B402 (planned), B225 (the legacy-era lab, built), B435, B23, B258, B263, B499, B503.
  ADR-088, ADR-175, ADR-230, ADR-231.
- Status: **in progress** (B225's lab exists; B402 carries it into horde 2). Ruled 2026-10-16
  (ADR-230 and its A1): the make-up gain is a user control (default ×4, +12.04 dB, restoring
  ADR-221's 12 dB) between the engine's output and the rack's first slot, and the gain-staging
  view gives every stage a gain control, a meter, a peak hold and a clip lamp that latches until
  cleared (spec RC-2, RC-8). Per-preset make-up by loudness is ruled for the bank stage (ADR-231,
  B499); an adaptive make-up mode waits for its own lab (B503, after B402).
- Needs first: `sub`, `noise`, `filters` (inferred: the strips are the sources, the routing
  places the filters).
- Open: how the rack-input clip lamp treats an over when it will light on most bank chords
  (ADR-230, still open by A1 item 4; B402); the make-up's range and the faders' laws (B402); whether
  the adaptive mode ships (B503); corner FX buses (B258).

**Master strip and limiter** (`master`). One instance of Bulwark's limiter face, never a rack
slot, the last stage before the output. The mixer page's master strip has limiter on/off,
Ceiling, Release, a gain-reduction meter, the clip latch, and a cut-only Volume.
- Governing: B438 (approved 2026-10-04, "master only for 1.0"), B402, B435. ADR-195 (Ceiling
  default −1.0 dBFS), ADR-190.
- Status: **ruled**. Bulwark mirrored the −1.0 default; the thread closed 2026-10-09. With the
  make-up before the rack (ADR-230) and no peak guarantee in the engine (ADR-226 item 8, spec
  OU-1), this limiter is what holds the device's peaks.
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
- Governing: B450, B439, B50, B281, B262, B393, B435, B429, B463. ADR-172, ADR-175, ADR-128,
  ADR-190, ADR-193, ADR-195, ADR-201, ADR-202, ADR-203, ADR-218.
- Status: **planned**. horde 2's rack is unbuilt. The hosted-module rack-slot contract is
  **RATIFIED** (`docs/proposals/rack-slot-contract.md`, ADR-201, 2026-10-09); Q3 and Q9 are
  provisional until measured. It takes FOUNDATIONS' FX-operator ABI as the code-level interface.
  Hosted modules' presets (ADR-202) and the module bar's readings (ADR-203) are ruled. Its 3 dB
  click ceiling is **suspended** as pass/fail until the first listening batch sets one that does
  not depend on the probe's frequency or the sample rate (ADR-218, answering Bulwark's readiness
  notice, B463). The engine's side is reserved: one stereo pair at zero latency, after the
  make-up gain (spec RC-5).
- Needs first: `mix` (inferred); `seams` (B281).
- Open: measure Q3 (the click ceiling, now with ADR-218's suspension) and Q9 (the state budget);
  build the admission test with the shell (B398). The module cutoff is set: Friday 2026-12-11 (ADR-236).

**Shriek, formerly MAW** (`maw`). The three-stage saturator, FX-C, a hosted sibling module.
Renamed by the human on 2026-10-04 (B433); the part id stays `maw`. B318, B373, B393, B433.
ADR-170, ADR-092, ADR-190. **In progress.** Needs `rack`. Its level contract is answered (B435).
The engine-rebuild-audit brief is with it (B494). Open: FX-C as a fixed post-stage or a rack slot
(the new engine builds a rack slot and no engine insert until spec Q23 is ruled, after the
build), and B318's other items.

**Sluice** (`sluice`). The morphable FX network, a sibling project. One patch per module XY;
different patches at horde's morph corners (ADR-188). B328, B321, B329, B337, B347, B353, B361,
B369, B436, B273, B359, B393. ADR-188, ADR-166, ADR-193. **In progress** (labs). Needs `rack`.
- Consumption is **ruled** on Sluice's side (Sluice D-096, recorded in B328 on 2026-10-04): horde
  takes Sluice's code into its own tree at Sluice V1, when Sluice goes public, with no tag and no
  lock file. The private-optional-dependency proposal is superseded.
- It answered the engine-rebuild-audit brief (B494): its human ruled a targeted clean-up, not a
  rebuild (Sluice D-139). Its question, which side of the slots the make-up sits on, is ruled by
  ADR-230: before them, so Sluice sees horde's earlier level.
- Open: the lab's tails (B369); ADR-193 D5 plans a per-module tail cap.

**Scape, the reverb** (`reverb`). Built as the sibling project Scape (B421, spun up 2026-10-02).
B421, B152, B210, B393. ADR-177, ADR-190, ADR-198. **In progress.** Needs `rack`. The
engine-rebuild-audit brief is with it (B494).

**ECHO** (`echo`). horde's standard delay, built in horde, not a sibling (B425, ruled 2026-10-04):
a clean stereo delay with tempo sync, ping-pong, mid/side, feedback filtering and modulation.
B425, B386. ADR-190, ADR-142. **Planned.** Needs `rack`.

**Bulwark compressor** (`bulwark`). The compressor face of the sibling project Bulwark (formerly
Dynamite), hosted in the rack; its limiter face is `master`. B421, B234, B423, B450, B463.
ADR-190, ADR-195, ADR-198, ADR-218. **In progress**: its M1–M3 are closed, and M4, hosting in
horde, waits on the slot contract (B450). Its readiness notice is answered (ADR-218, B463:
crossfade its two detector switches; a declared tail may mean the time until the instance is at
rest). The engine-rebuild-audit brief is with it (B494). Needs `rack`.

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
`com.mindlathe.horde`, a new AU subtype, its own bundle, pkg identifier and preset root, the
vendor "Mindlathe", the display name "Horde"). Parameters are generated from manifests into a
committed lockfile; the state carries a schema version from day one.
- Governing: B398 (planned), B428 (frozen identities from the first release), B429 (process,
  tails and the wrapper boundary), B449 (R1–R11: MIDI input never stalled by GUI activity), B305,
  B308, B306, B458 (the playable preview). ADR-186, ADR-197 (a clean break from legacy saved
  state), ADR-200, ADR-207, ADR-209, ADR-211, ADR-215, ADR-223, ADR-229.
- Status: **ruled**. The skeleton is not built. **Build first** (ADR-215): a playable preview, a
  thin plugin around the current C++ engine under its own identity ("horde 2 PREVIEW"), is BUILT
  and INSTALLED (B458, 2026-10-12), deliberately not the B398 skeleton and skipping ADR-186 §5's
  rules, which are owed before the first release. The skeleton's acceptance list is drafted
  (`docs/proposals/h2-shell-acceptance.md`, 42 rows); flush-to-zero (ADR-211), the latch-and-report
  output guard (ADR-209), the thread-handoff rules (ADR-200) and the host-boundary checks
  (ADR-207) are ruled for it. It will host the new engine (ADR-223), which reserves what a shell
  needs: a lock-free GUI readout snapshot (RC-8), append-only parameter ids with unknown keys kept
  (RC-9) and history support (RC-11).
- Needs first: `seams`; `engine2` (was `port`).
- Open: which spec the rest of horde 2 (the shell, the rack, GUI 3) answers to (B308 M2); the
  engine's is `specs/SPEC-ENGINE.md` (ADR-224 item 8). The AU manufacturer code (ADR-186 §3).
  Ratifying the shell acceptance list with B398 (ADR-209, package P17).

**Legacy freeze tag** (`freeze`). The human cuts the legacy tag; CI archives the universal signed
bundles; old projects depend on that archive (ADR-186 §2). Legacy sessions stay on the old system
(ADR-197).
- Governing: B255, B308 (H1, H5, M4), B305, B456. ADR-186, ADR-207, ADR-208, ADR-209. Status:
  **built** (2026-10-17). The tag `v0.9-legacy` was cut on main `57d61f9`; CI built, signed
  (ad-hoc) and drafted the release; the human published it. Its assets are the legacy plugin of record. A side-by-side test identity exists for trying legacy changes
  without replacing the installed plugin (B456). Needs first: nothing in this graph.
- Open: cut the tag. (The real-blob corpus of B308 H5 is skipped: ADR-209 item 11.)

**Testing apparatus** (`testing`). The patch-space gauntlet, the blind listening passes, the
aliasing metric, the Serum 2 reference test and the C++ quality suites.
- Governing: B316, B324, B340, B344, B350, B381, B186, B457, B490. ADR-187, ADR-209, ADR-227.
  Status: **in progress**; Serum stage 1 merged (#886); the engine harness and nightly runner
  (B457). For the new engine, the acceptance tests (SPEC-ENGINE §10.1 items 0–23, with §8's and
  §9's checks) are written from the spec alone by a different agent before the engine exists
  (B498, waiting on the test-author session of B502), calibrated on the 380 reference renders
  (B497). The click detector is ratified at k 10 (ADR-227, B490), the test author's first
  instrument.
- Needs first: `engine2` (B498; was `port`).
- Open: loading Serum 2 after stage 1 (B381); the aliasDb re-ruling (B350).

**Performance** (`perf`). C++ CPU against the B439 budget (sources ≤ 34 % of a min-spec core for
8 voices; CPU is Layer-E, never a gate, ADR-187 §8), the voice limit, oversampling as a quality
option, a WASM build for the labs, R2 and R3, FX cost.
- Governing: B441 (the CPU campaign), B445 (os 1 default, os 2 HQ), B323, B372, B375, B357, B262,
  B488. ADR-187, ADR-191, ADR-209, ADR-220, ADR-225.
- Status: **in progress**. Specialised kernels landed bit-identical (B441 phase 2); the os 1
  default flip is built (#984), with the ADR-197 migration; the current engine may change its
  sound for speed when heard A/B (ADR-220, B488). The target is restated for the new engine (B488,
  B494): the heaviest bank patch at about 10.7 % of an M3 performance core per swarm at 8 voices;
  the rebuild audit estimates a clean engine at about 11.5 % (B493). Polyphony is ruled: up to 64
  voices preallocated, default 16, no CPU-adaptive limiting by default, the budget still measured
  at 8 (ADR-225 item 13).
- Needs first: `engine2` (was `port`, `edges`).
- Open: the WebAssembly toolchain's exact version (approved in principle, ADR-225 item 10; B372);
  R2 and R3 (B357); the heavy-patch policy (B441 phase 3; B488's target is the lead's arithmetic,
  for the human to confirm); one CPU measurement on the human's Windows PC (ADR-209 item 10).

**Blind-Spot Armor and security gates** (`armor`). Every risk-register row is a named gate in
`./verify` or a visible hole on the dashboard; correctness and security share one catalogue.
Includes the runtime safety nets (B430).
- Governing: B448, B446, B430, B455, B457. ADR-197, ADR-194, ADR-196, ADR-199, ADR-200, ADR-206,
  ADR-207, ADR-209 to ADR-215.
- Status: **in progress**. It has been law since 2026-10-08. Phase 1 (the retrofit) is complete.
  Phase 2 is ruled (ADR-209: engine now, shell at birth, legacy frozen) and its Wave 1 is built
  and merged (2026-10-11). Score on 2026-10-11: 0 of 12 risk rows fully gated, 12 partial, none
  missing; at ratification it was 0, 11 and 1 (soak). Since 2026-10-12 new armor work waits
  (ADR-215 item 4, "build first"): gate rows found since are filed and not chased (B470, B473,
  B492, B495, B496). This refresh did not re-measure the score.
- Needs first: nothing.

**Conformance, distribution and legal** (`distrib`). What a release must clear outside the DSP:
pluginval, auval and clap-validator on the signed builds (B427), the Windows host test before any
Windows release (B447), signed installers, licensing and the update path (B412), trademark and
licence items (B433). Platforms: macOS and Windows x64 (B432).
- Governing: B427, B447, B412, B433, B471, B472. ADR-190, ADR-196, ADR-204, ADR-205, ADR-209,
  ADR-219. Status: **planned**. Needs first: `shell`.
- The licence direction is ruled (ADR-219, B471): horde stays public, all rights are reserved
  until 1.0, and 1.0 is released under the GPL. A prior-art and legal read of blade synthesis
  comes before any build is offered for money (B472). Intel macOS is not a 1.0 platform (ADR-209
  item 8).
- Open: copy protection, signed installers and the update path (B412); the GPL's version and the
  copyright holder's formal name, settled when the licence file is written (ADR-219 item 3); the
  counsel items (B433) and the pre-commercial legal read (B472).

**GUI 3 and the screens** (`gui3`). The labs assembled into a new GUI that binds only to
generated manifest keys (B308 L2); FX modules as software on screens; the settings page and file
manager. The GUI never interferes with playing; typed entry opens from double-clicking a value
readout, and double-clicking a knob resets it (B449); accessibility at Surge XT's level (B432).
- Governing: B302, B449, B432, B271, B322, B336, B261, B308. ADR-116, ADR-186, ADR-190, ADR-229,
  ADR-230.
- Status: **planned**. The screens workshops are built; the settings lab (B261) has not started.
  The new engine publishes a lock-free readout snapshot for the GUI (RC-8) and the meter readings
  the gain-staging view draws (ADR-230 item 4).
- Needs first: `shell`, `fxmorph`, `intent`, `modmorph`, `mix`, `maw`, `sluice`.
- Open: one screen style for every FX module, or one each (B322).

**Presets and the factory library** (`presets`). Global presets that cover everything the synth
can do; saving one also ingests each of its corners; every FX module's presets roped in. The
preset format writes `os` explicitly or carries a format version (B395, ADR-197).
- Governing: B395, B257, B261, B312, B349, B453. ADR-186, ADR-167, ADR-197, ADR-202, ADR-169,
  ADR-226, ADR-231. Status: **planned**. The bank's levels are evened by loudness at the build's
  bank stage (ADR-231, spec OU-2); velocity and aftertouch are mapped by default in the factory
  presets (ADR-226 item 4).
- Needs first: `history`, `testing`, `intent`, `gui3`, and every 1.0 FX module (`maw`, `sluice`,
  `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`).
- Open: what "everything that fits under corner rule" means (B395); commissioning sound designers;
  the bank's coupling rescale and the negative-coupling presets' starts, by ear in the preset
  round (ADR-226 items 12, 13; spec X23, X24).

**User manual** (`manual`). The horde 1.0 manual in `docs/manual/`: 20 chapters, 244 sections and
a 233-entry figure registry, scaffolded 2026-10-09 (#1009). Writing the prose is not dispatched.
- Governing: B451, B413. ADR-186, ADR-190. Status: **in progress**. Needs first: `gui3` (its
  screenshots).
- Open: the lead's defaults, including "the manual is for 1.0", are open to the human (B451).
- Its registry cites `engine` 33 times and `port` 19 times; both are superseded by `engine2` in
  this refresh, and the ids are kept so the registry still checks. Re-pointing those entries is
  the manual's next revision.

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
| A1 | **The new engine** (ADR-223, B494), replacing the clean port: the composed engine is rebuilt from a user-level spec in a clean room, not ported. DONE: the rebuild audit (B493), the spec (ratified by #1097), the clean-room tree and the 380 reference renders (B497), the two history-free repositories (B502). NEXT: the human opens the test-author session; the acceptance tests by a different agent (B498); then the build in BD-8's stages, the labs on WebAssembly; judged on the bank A/B, the CPU target (B488) and the invariant suite; then cutover. Meanwhile the current engine plays in the preview (B458, ADR-215). | `engine2`, `testing`, `perf` |
| A2 | The shell skeleton (ADR-186 §5), after the preview that came first (ADR-215, B458): the new identity; the manifest feeding a generated parameter lockfile (B308 H4), now the new engine's manifest (spec §5.1, append-only by RC-9), with B277 re-scoped to horde 2; the state schema version; the counter-based draw ADR (B308 H3). Frozen identities (B428), the wrapper boundary (B429) and the GUI rules R1–R11 (B449) land with it. | `seams`, `shell` |
| A3 | History on the skeleton (B389), ported as soon as horde 2 has state, its acceptance grown as each feature lands; the engine's side is RC-11 (ADR-229 A1). | `history` |
| A4 | The pitch seam: the bend laws and glide on the engine's per-note pitch input and published pitch (RC-3, ADR-225 item 7), with B504's glide system after the engine's stage 2 (on PR #1117); the quantizer as a service, MTS-ESP with its stated precedence, and MPE through CLAP per-note expressions; room for Tonality (B390). | `bend`, `quant`, `mts`, `mpe` |
| A5 | The gates 1.0 must pass: Blind-Spot Armor (ADR-197; phase 1 done, phase 2 ruled, ADR-209, with new armor work waiting while horde builds first, ADR-215 item 4), the conformance gates once a signed shell exists (B427), the GPL at 1.0 and the pre-commercial legal read before any paid build (ADR-219, B472). | `armor`, `distrib` |

**Track B — lab sync** (human-paced, one lab per review session; B331 reordered):

| Stage | Was in B331 | Work | Parts |
|---|---|---|---|
| S0 | S0 | Triage. Done in substance: the navigator archives the dead-engine and superseded labs (B407). | `archived` |
| S1 | S1 (part) | Engine decisions. **Done in substance:** B376 and B386 are decided (2026-10-02/03); the edge correction is ruled (ADR-222); the SCALPEL interface became the new engine's ratified spec (ADR-223 to ADR-229), with envelope timing ruled (ADR-226 item 10). Left: the spec's questions ruled during the build, each before its stage (§11.1, among them Q31, the 19 deferred rows and the default curves). | `audit`, `engine2`, `modulators` |
| S2 | S2 (part) | **Modulation, promoted:** the modulators and envelopes lab, then the mod matrix lab (B392, with B57's spring source), then the modulator-morph workshop (B396). | `modulators`, `modmatrix`, `modmorph` |
| S3 | S2 (part) + S4 (head) | **Morph, with the FX algorithm morph promoted**: the morph editor with stepped morph glide (B424), the FX algorithm morph (ADR-193; rounds 2–3b built), the patch model (B263, the state schema), then the intent slots (ADR-192). | `morph`, `fxmorph`, `patch`, `intent` |
| S4 | S1 (part) + S3 | Sources, routing, stereo and performance input: the Sub workshop, the noise oscillator, filters, the stereo lab (B408), the mixer and routing with the master strip and the gain-staging view (B402, B438, ADR-230), then the adaptive make-up lab (B503), the MPE design (B388) and the arpeggiator. | `sub`, `noise`, `filters`, `stereo`, `mix`, `master`, `mpe`, `arps` |
| S5 | S4 | FX modules and screens: the rack-slot contract first (B450, ratified, ADR-201), then the hosted siblings (Shriek, Sluice at V1, Scape, Bulwark) and horde's own (ECHO, EQ, FX filter, drive); the conditionals by the module cutoff; then the screen aesthetic and logos. | `rack`, `maw`, `sluice`, `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `ott`, `kchorus` |
| S6 | S5 | Presentation, then GUI 3. The settings lab (B261) comes first; B449's rules and B432's accessibility level apply. | `gui3` |
| S7 | S6 | Calibration and content: the Serum gauntlet (B381) and the listening pass, the random patch P4/P5, the legacy preset import (B312), the presets lab and factory bank (B395, B257), and the manual's prose and screenshots (B451). | `testing`, `presets`, `manual` |
| 1.0 | — | The legacy tag is cut and archived before the human moves their work (ADR-186 §2, §6). Modules that miss the cutoff ship after 1.0 (ADR-190 A1). Then the stability line. | `freeze`, `release` |

Why the Sub and filters sit in S4, not S1 where B331 put them: the new engine's spec already
gives the sub, the noise oscillator and the filter slot their places in the voice (5.5, 5.6,
RC-1), so these sessions design what fills them (B399, spec X15, X13), and before the stability
line the manifest may grow (ADR-186 §5(e), (f); RC-9 keeps that safe). The mod matrix and morph
labs sync against the parameters first. It is a proposal for the human.

## Critical path to 1.0

The longest chain of completion prerequisites among the parts that are **in** for 1.0, computed by
the map from the table above. The map recomputes it on every load, and its self-check 5 proves
that no longer chain exists. As of this refresh, 10 parts (12 on 2026-10-09). The map highlights:

`audit` → `seams` → `sub` → `mix` → `rack` → `sluice` → `fxmorph` → `history` → `presets` →
`release`

What it says:
- **It is shorter by two because the rebuild re-drew its head.** The superseded `engine` no longer
  starts it, and `sub` and `filters` no longer wait on the bend laws: the one published per-voice
  pitch they needed is the new engine's (ADR-225 item 7; RC-1, RC-3). The bend laws now hang off
  `engine2` and feed `quant`, `mts` and `mpe`, a shorter chain.
- **It is one of twelve tied chains of 10** (enumerated 2026-10-17 by a script over the map's
  data): the second link is `seams` or `engine2`; the third is `sub`, `noise` or `filters`; the
  eighth is `history` or `gui3`. So the new engine IS on the critical path, tied with the seam
  gate: shortening the path means shortening every alternative, and the map highlights only the
  first it finds.
- **The head of the path is unpaid work with no dispatch.** The audit is decided (`ruled`) except
  spec Q31. The seam gate (B275 (a), (c), (d), B277) is not dispatched; the new engine is at its
  tests (B498).
- **The long pole is still sources → routing → rack.** The Sub workshop (B399), the noise
  oscillator's design (B452, spec X15) and the mixer lab (B402) have not started, though the
  rack-slot contract is ratified (B450, ADR-201) and the gain staging is ruled (ADR-230).
- **Sluice is on it.** The FX algorithm morph cannot finish without ADR-188's structural class for
  Sluice patches, and Sluice's code enters horde only at Sluice V1, when Sluice goes public (B328,
  Sluice D-096). That date is Sluice's, not horde's.
- **History is near the end of the path,** not because it starts late (A3), but because its
  acceptance lists every feature it must survive.
- **Three of its edges are inferences** (`sub`, `noise` or `filters` → `mix`, and `mix` →
  `rack`), drawn dashed. If the human or the lead rejects one, the path changes.
- Parts that are conditional, open or superseded (`ott`, `kchorus`, `manual`; `engine`, `edges`,
  `port`) are left out of the computation.

## What 1.0 is

1.0 is **horde 2's first human-cut release tag: the stability line** (ADR-186 §5(f)). Before it,
laws, parameters and ranges change freely, and projects saved earlier may change sound. After it,
a sound-law change is selected by the state header's version, and parameter ids and normalized
ranges are locked by the lockfile.

**The roster froze on 2026-10-07** (ADR-190, with Amendments 1 and 2). Three dates, not one: the
roster locked at the freeze; each module's scope list freezes at its lab sync; the MUST rows of
the module 1.0 bar (B439) are judged at the module cutoff, **Friday 2026-12-11** (ADR-236), with horde-side rows judged at hosting. A module that misses
the cutoff ships after 1.0, and anything undecided at the freeze went after 1.0 by default (A2).

What ships in it, by ruling (ADR-190 unless noted):
- **Sources:** the new engine (ADR-223, B494): one instrument-wide voice per note holding two
  swarm oscillators with their blades, the sub and the plain noise oscillator (ADR-224 item 1;
  B327; A6), up to 64 voices, default 16 (ADR-225 item 13); the new Sub's design (B399); the
  filters.
- **FX:** Shriek; Sluice; Scape; ECHO; Bulwark's compressor; EQ, the FX filter and a simplified
  drive module (A7, A8).
- **Master:** Bulwark's master limiter on the master strip (B438).
- **Modulation and performance:** the modulators and envelopes, the mod matrix, modulator morph
  (B396), the macros with the intent bus (ADR-192), morph with stepped morph glide (A12) and the FX
  algorithm morph (B394), MPE plus channel aftertouch (A4), the quantizer and a simple arpeggiator
  (A5), MTS-ESP (B431), the bend laws (the human's summary, B397).
- **Platform:** true stereo (A9), history and undo (B389), macOS arm64 and Windows x64 (B432;
  Intel macOS is out, ADR-209 item 8), and accessibility at the approved level (B432).
- **Presets and GUI:** the presets and factory library (B395) and GUI 3 (B302).
- **The shell:** the horde 2 shell with its own identity, as CLAP, VST3 and AU (ADR-186 §3).
- **The licence:** 1.0 is released under the GPL; until then all rights are reserved (ADR-219).

**Conditional:** the Kuramoto chorus (A3), and Bulwark's multiband mode with the ATM preset
(B423). **Out:** see [Post-1.0](#post-10).

The gates it passes:
- `./verify full` green, which now includes the Blind-Spot Armor gates (ADR-197);
- the readiness gate on every source (B275);
- the new engine's truth standard: its ratified spec, its Layer-0 invariant suite (spec §10.1)
  and the human's ears on the bank (ADR-224 item 8); any other core keeps its ADR-187 status,
  parity-proven or demoted with goldens and a Layer-0 suite;
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

## Superseded

Kept as parts, with their ids, because the manual cites them; nothing depends on them.
- **Composed engine** (`engine`), **clean C++ port** (`port`): replaced by the new engine
  (`engine2`) under ADR-223. The composed engine plays until cutover, under ADR-187 (ADR-223
  item 6).
- **Edge correction** (`edges`): ruled option (d) (ADR-222); its build parked (B491, ADR-223
  item 5); carried into the new spec's ED rows.

## Open human decisions

One list, grouped by part, and the same set as the map's (each part's "open human decisions").
The row is where the answer is recorded. Renumbered in this refresh: cite the row, not the number,
because the numbers move.

Answered since 2026-10-01 and removed: the legacy roundup (B386, ratified 2026-10-03); the
bit-exact floor (ADR-187 A2); MPE in 1.0 (ADR-190 A4); the FX-morph I3 reading, STRICT or TAIL and
the approved set (ADR-193); Sluice's consumption mechanism and binary distribution (B328, Sluice
D-096); OTT and the Kuramoto chorus in 1.0 (ADR-190 A3, B423); arps and sequencers (ADR-190 A5);
a noise oscillator in 1.0 (ADR-190 A6); the freeze tag's real-blob corpus (skipped, ADR-209 item
11).

Answered since 2026-10-09 and removed (their numbers in the 2026-10-09 list in brackets):
- [4] **edge correction:** option (d), the minimum-phase table (ADR-222, B383);
- [8] **JS demotion:** the new engine is held to no JS parity and the JS twin is retired for
  WebAssembly (ADR-223 items 3, 4; ADR-224 item 8); ADR-187 governs the composed engine only until
  cutover (ADR-223 item 6);
- [27] **mixer taps:** every stage gets a meter, a peak hold, a latching clip lamp and a gain
  control (ADR-230 item 3, A1);
- [38] **voice limit:** up to 64 voices preallocated, default 16, no CPU-adaptive limiting by
  default (ADR-225 item 13);
- half of [3] **envelopes:** what a time knob means: a stage's time is exactly when it ends
  (ADR-226 item 10). The default curves remain, in spec Q31 (item 2 below);
- half of [26] **true stereo:** per voice or post-sum: members stay mono sources panned across the
  field, each voice is a stereo pair through the filter slot, and true-stereo processing follows
  the voice (ADR-224 item 3). The width candidates remain (item 30);
- **the make-up gain, a control or fixed** (new since, ADR-230): a user control, default ×4
  (ADR-230 A1).

Narrowed, not removed: [2] the 19 deferred rows are now spec Q31 (item 2); [35] which spec horde 2
answers to is answered for the engine only (item 38); [39] the WebAssembly toolchain is approved in
principle (item 45); [45] the commercial model's licence half is ruled (ADR-219; item 57). Kept:
[5] ADR-189's D1–D3 defaults (item 6): no record says the new spec absorbs them, so the reading
that it does stays a proposal for the human; [6] and [7], the `verify full` runtime and the floor warning (B406): the composed engine's gates
run until cutover, and no record retires them with the port.

1. **Order of work:** approve or reorder the lab-sync order (B331; this plan's proposal above).
2. **Engine audit:** the 19 deferred rows, now spec Q31 (keep, lock or cut each, and envelope
   decision 1's default curves), ruled during the build (SPEC-ENGINE §11.1; B376).
3. **Deferred tests:** B420's deferred tests (basin, onset scatter, per-member envelopes, …).
4. **New engine, the spec's open items:** its 25 questions to rule during the build, each before
   the stage that needs it, and 9 after it (SPEC-ENGINE §11.1, §11.2; B494).
5. **New engine, the gaps B500 exposed:** where the oscillators return to the host rate; where
   IF-1's per-note volume and pan expressions act; who receives VO-10's lifecycle and IF-1's load
   readouts (ADR-232, PROPOSED on PR #1117, names the pitch system as the lifecycle's consumer;
   the load readouts' consumer stays open).
6. **ADR-189 defaults:** DONE 2026-10-17 (ADR-237 item 10): superseded by the new engine's spec; ADR-189 stays as the old engine's history.
7. **verify full runtime:** a budget or a split into staged gates; a `./verify` edit (B406 item 4).
8. **Floor warning:** should the floor's key-mismatch warning fail the gate (B406 item 5).
9. **Seams:** B275 (a) the seam list and "ready" (whether the spec's RC-1 to RC-11 answer it),
   (c) the FOUNDATIONS / horde split, and (d) proceeding on the provisional schema.
10. **Poly glide:** Deferred by the human 2026-10-17; the glide system is B504 (ADR-232 leaves the engine room for poly glide, the spring law and quantize). Their reference renders do not glide (B497).
11. **Scale sidechain:** DONE 2026-10-17 (ADR-237 item 8): after 1.0.
12. **MPE:** its design (B388); which expressions reach the swarm and the blades (spec Q2).
13. **Note-on randoms:** the form of the linked pair (B264).
14. **ORBITAL velocity:** BACK TO THE LAB 2026-10-17 (ADR-235 item 4), with the human's idea of a 10×/0.1× mass toggle; not ruled.
15. **Onset scatter:** the recommendation (B370); where it lives is spec Q13.
16. **Modulator morph:** a morph law per modulator type (B396).
17. **Morph editor:** B269's items: per-group cohesion, boundary editing, exempt and locks.
18. **Counter-based draws:** ratify the draw ADR before the first horde 2 patch is saved (B308 H3).
19. **Intent phrases:** the phrase list for the named slots 5–8 (ADR-192).
20. **Intent spec:** approve the SPEC-INTENT-BUS amendment for §3.1 and principle 2 (ADR-192).
21. **The legacy Sub:** what the human dislikes in it (B327, B399).
22. **Noise oscillator:** its design (spec X15; its design lab, B452).
23. **Filters:** re-ruling the filter tolerances physics refuses (B287 → B290).
24. **Per-note filters:** the type list, after the Release cost measurement (B303).
25. **Filter count and routing:** in the voice (spec X13).
26. **Rack-input clip lamp:** DONE 2026-10-17 (ADR-235 item 3): the rack-input lamp latches an over like every other stage.
27. **Make-up range:** the make-up gain's range and the faders' laws (B402; ADR-230 A1 item 2).
28. **Adaptive make-up:** whether it ships, after its lab's A/B (B503).
29. **Corner FX buses:** the discussion is owed (B258).
30. **True stereo:** which width candidates are in 1.0, and the mono-fold guarantee (B408; spec
    Q26).
31. **Rack-slot contract:** Q3 and Q9 by measurement, and the slot-switch click ceiling by ear in
    the first listening batch, which also replaces the ceiling ADR-218 suspended (ADR-201, ADR-209
    item 13, ADR-218).
32. **Module cutoff:** DONE 2026-10-17 (ADR-236 item 1): **Friday 2026-12-11**; horde-side rows are judged at hosting (item 2).
33. **Shriek:** FX-C as a fixed post-stage or a rack slot (spec Q23 builds a slot until then), and
    B318's other items (B318, B435).
34. **Sluice tails:** label, stand-in gate, or leave (B369).
35. **Module names:** PARTLY ANSWERED 2026-10-17 (ADR-235 item 1): one glass each. Still open: the names themselves, from B336's three to five per module.
36. **FX algorithm morph:** p and K by ear (ADR-193 D8), and the items B265 still lists.
37. **Patch model:** ratify it (B263).
38. **Spec:** which spec the rest of horde 2 (the shell, the rack, GUI 3) answers to (B308 M2);
    the engine's is `specs/SPEC-ENGINE.md` (ADR-224 item 8).
39. **AU code:** PROPOSED 2026-10-17 (ADR-235 item 5): `Mndl`, as the fleet code for unreleased Mindlathe plugins; autonomous to rule (its brief hypersaw-017).
40. **Shell acceptance:** ratify the drafted list with B398 (ADR-209, package P17).
41. **Freeze tag:** DONE 2026-10-17. `v0.9-legacy` is cut and published (ADR-186 §2, B255); kept here so the numbers do not move.
42. **History scope:** DONE 2026-10-17 (ADR-235 item 2): confirmed, and loading a preset into a module is history too; hand edits versus DAW automation to be measured per host.
43. **Serum 2:** load it after seeing stage 1 (B381).
44. **aliasDb:** the re-ruling (B350).
45. **WebAssembly:** the toolchain's exact version, at the lab stage (approved in principle,
    ADR-225 item 10; B372).
46. **R2 and R3:** rule after the critic (B357).
47. **Heavy patches:** the policy for the worst known patch, over budget today (B441 phase 3);
    B488's restated target is the lead's arithmetic, for the human to confirm.
48. **Windows CPU:** one measurement on the human's Windows PC, replacing the assumed ×1.5
    slow-machine factor (ADR-209 item 10).
49. **Screen styles:** DONE 2026-10-17 (ADR-235 item 1): one style per module, the simplest may share the house style, plus a backup UI in horde's house style for each.
50. **Corner auto-ingest:** what "everything that fits under corner rule" means (B395).
51. **Sound designers:** whether to commission them for the 150–400-preset norm (B395).
52. **Bank coupling:** the coupling rescale and the negative-coupling presets' starts, by ear in
    the preset round (ADR-226 items 12, 13; spec X23, X24).
53. **Machine account:** create it for agent PRs, then switch on code-owner review (ADR-210).
54. **Nightly workflow:** install it from the draft in `tools/nightly/` (ADR-209 item 4).
55. **Armor thresholds:** ratify each wave's thresholds table (ADR-209 item 5).
56. **Golden pins:** do bit-identity pins count as goldens? (the phase 2 proposal, section 7).
57. **Commercial model:** copy protection, signed installers and the update path (B412); the
    GPL's version and the copyright holder's formal name, when the licence file is written
    (ADR-219 item 3).
58. **Counsel items:** trademark screening and licences (B433), and the pre-commercial legal read before any
    paid build (B472).
59. **Manual:** the lead's defaults, including "the manual is for 1.0" (B451).
60. **Shriek's three questions** DONE 2026-10-17 (ADR-237 items 1–3): the rack runs at the host rate; oversampled factory defaults are allowed within 4 %; Shriek's latency is allowed as for Sluice.
61. **Sluice library:** DONE 2026-10-17 (ADR-237 item 7): yes, read-only.
62. **Automated-parameter budget:** DONE 2026-10-17 (ADR-237 item 6): 4 %.


## Inconsistencies found

Flagged for the lead, not resolved here. Each carries its state: **RESOLVED** (fixed in the
records), **TRACKED** (owned by a row, not yet done) or **OPEN** (no row owns it). Items 1–22 are
from 2026-10-01 and 2026-10-09, re-checked on 2026-10-17 against main `3f338ce`; items 23–29 are
new in this refresh.

1. **RESOLVED.** Rows B385–B402 existed only on `lead-records-158`. PR #888 merged, and the map's
   check 1 reads every cited row.
2. **RESOLVED (ecbb205).** B379's status marker was stale; it now reads "STEP 1 MERGED (#876);
   SUPERSEDED by B385".
3. **RESOLVED (by B385/B405).** `h2/README.md` was stale against B385. It is now "Last verified:
   2026-10-01" with B405's wiring, and the stale phrases (the razor_core wait, "no divergences
   yet") are gone.
4. **TRACKED under B403, not yet done.** The charter's §Domain (CLAUDE.md) still predates ADR-186,
   B327 and B330: it presents SCALPEL as a CANDIDATE whose "integration shape ... is the open
   question" and CANTO as the first new member, and it still says MAW, renamed Shriek (B433).
   (Its domain invariant does carry ADR-224's scope for the new engine.)
5. **TRACKED under B403, not yet done.** B302 still frames GUI 3 as a switch-over from gui2, which
   belongs to the frozen legacy shell.
6. **TRACKED under B403, not yet done.** Legacy-tree rows B277, B278, B282, B288 and B289 are
   still not re-scoped per ADR-186's consequences. B277 now also predates the new engine's
   manifest (SPEC-ENGINE §5.1), and B278's published pitch is the new engine's (ADR-225 item 7).
7. **OPEN.** The B331 order is still "awaiting the human", while ADR-190 A1 already freezes scope
   lists "at its lab sync (B331)". The plan's order is a proposal, not a ruling.
8. **OPEN.** The freeze has different prerequisites in different records: ADR-186's note (after
   B255, which merged as #808), B308 ("H5's corpus and H1's tag come before the freeze") and
   ADR-186 §1(α). ADR-197's "legacy sessions can stay on the old system" does not settle it.
   ADR-209 item 11 skips H5's corpus, so B308's sentence is now half stale.
9. **TRACKED under B403, not yet done.** B263 carries no BUILT marker, though its lab is built.
10. **RESOLVED (ADR-223 item 4, ADR-225 items 10 and 13, B494).** The WASM build and horde 2's
    voice limit had no row of their own. The labs run the new engine as WebAssembly (BD-8 stage
    8, under B494), the toolchain is approved in principle, and the voice limit is ruled.
11. **RESOLVED (ADR-190).** The 1.0 FX list differed between B327 and B393. ADR-190 rules it.
12. **RESOLVED (ecbb205).** B391 deferred sequencing to the plan of record, which cannot rule.
13. **RESOLVED (2026-10-09).** ROADMAP's head section "1.0 — DEFINITION OF DONE" now carries
    "Superseded for horde 2 (noted 2026-10-09)".
14. **RESOLVED (2026-10-09 consistency pass).** B327 and B409 now note ADR-190 A6.
15. **RESOLVED (2026-10-09 consistency pass).** B391 now notes ADR-190 A5.
16. **RESOLVED (2026-10-09 consistency pass).** B393, B400 and B401 now note ADR-190, B423 and the
    Shriek rename.
17. **TRACKED under B452.** The noise oscillator, EQ, the FX filter and the drive module have no
    row of their own; B452 holds them until work starts.
18. **RESOLVED (2026-10-09 consistency pass).** B405, B416 and B385 now carry notes of their real
    state (their first markers are unchanged, as rows are appended to).
19. **RESOLVED (2026-10-09 consistency pass).** B439 and B423 now read "by the module cutoff".
20. **RESOLVED (2026-10-09 consistency pass).** B425 drops "the four-role face".
21. **RESOLVED (2026-10-09 consistency pass).** B435 now cites B318 for the FX-C question. (This
    refresh renumbers the decision list again; it is item 33 now.)
22. **RESOLVED in part (2026-10-09 consistency pass).** B331 notes that S0 is done in substance and
    S1's B376 and B386 are decided. Since then S1's edge listening and SCALPEL interface are also
    done (ADR-222; the spec), which B331 does not say; the order itself still awaits the human
    (item 7).
23. **OPEN.** B504 and ADR-232 are not on main. They sit on records PR #1117 (open), so this plan,
    whose map checks citations against main's files, names them in prose only. When #1117
    merges, add B504 to `bend`'s rows and ADR-232 to `engine2`'s ADRs, in this file's table and
    the map's `DATA` together.
24. **OPEN.** Rows whose open questions the rebuild answered or moved carry no note saying so:
    B323 (the voice limit, ruled by ADR-225 item 13); B366 and B377 (envelope decision 1: timing
    ruled by ADR-226 item 10, the curves moved to spec Q31); B376 and B420 (the 19 deferred rows,
    now spec Q31); B380 (note-dependent edge correction, superseded by ADR-222); B404, B406 and
    B385 (the port, superseded by ADR-223; B404's suites move to spec §10.1); B408 (per voice or
    post-sum, ruled by ADR-224 item 3); B225 (the 18 taps, ruled by ADR-230); B372 (the WASM
    toolchain, ADR-225 item 10); B308 M2 (the spec, answered for the engine by ADR-224 item 8);
    B402 (still asks whether the make-up is a user control, which ADR-230 A1 answered).
25. **OPEN.** B497's marker reads "OPEN 2026-10-16", though its renders are delivered (PR #1110),
    its clean-room tree is on main (`engine2/`) and B502 built the repositories from it. It may
    be done; B498 still reads "Waits on B497".
26. **OPEN.** B399 (the Sub workshop) and B452 (the noise oscillator: "no work has started; it
    needs its design lab", 2026-10-14) predate the ratified spec, which now specifies a sub (5.5,
    the SN rows, ADR-225 item 6) and a minimum noise oscillator (5.6, SN-4, X15) inside the new
    engine. Neither row says whether its workshop designs within the spec's place or replaces
    it.
27. **OPEN.** `specs/ACCEPTANCE.md` L0-13's "master peak ≤ 1.0 post-tanh" does not bind the new
    engine (ADR-226 item 8: "to be scoped with the human's sanction, as L0-1 was"). L0-1 carries
    its ADR-224 scope note; L0-13 carries none yet.
28. **OPEN.** `specs/SPEC-ENGINE.md` is the new engine's ratified contract (B494, ADR-224 item 8),
    and ADR-230 item 5 treats an edit as "a change to a ratified spec, sanctioned by this ruling",
    but the charter's protected-paths list does not name it. Whether it is a protected path is the
    human's call.
29. **OPEN.** `docs/manual/figures.json` cites the superseded `engine` (33 entries) and `port`
    (19). The ids are kept so `manual_scaffold_check` holds; re-pointing those entries at `engine2`
    belongs to the manual's next revision (B451).

## Keeping this true

- The lead updates this file when a ruling changes a part's status, rows or prerequisites, and
  updates the `DATA` block in `docs/design/h2-plan-map.html` in the same change. The map's
  self-check 4 fails if the two disagree.
- A part id is renamed or retired only together with `docs/manual/figures.json`;
  `manual_scaffold_check` fails otherwise. A part that is replaced is marked **superseded** and
  keeps its id until the manual stops citing it.
- The map reads row titles and statuses live from ROADMAP.md. A part's status here is this plan's
  summary of those rows. When they move, the "Last verified" line above is what dates this
  summary.
