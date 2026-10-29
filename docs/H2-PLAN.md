# horde 2 — plan of record

**Last verified: 2026-10-27**, every part, against ROADMAP.md and DECISIONS.md on main `934449a`
(the merge of records PR #1229): rows B504 to B559, ADR-232 to ADR-294 with their amendments.
ROADMAP B387. This refresh covers everything since the follow-up of 2026-10-17, including the two
partial edits made since (ECHO a sibling, ADR-249, 2026-10-19; the arps entry, ADR-264 and B539,
2026-10-21): the clean-room build from stage 2 to the bank stage, the sub and noise workshop, GUI
3's design system and labs, Arp, the FX device after 1.0, the approved lab-sync order (ADR-292),
and the decisions answered since.

The earlier verifications are in git history: 2026-10-17 (main `3f338ce`, through B503 and
ADR-231, with its follow-up on main `a94e97e` through B511 and ADR-236), 2026-10-09 (main
`b2177c2`, through B451 and ADR-199), its partial refresh of 2026-10-11 (main `6dd5884`, through
B457 and ADR-212, ten parts), and 2026-10-01 (records PR #888, `ecbb205`).

This is the README-level orientation for horde 2: every part, the rows and ADRs that govern it,
its status, what it needs first, the order of work, and what 1.0 is. It is a MAP of the records,
not a second copy of them.

- **ROADMAP.md outranks this file.** Task state, acceptance criteria and rulings live in the rows
  cited here. Where this plan and ROADMAP disagree, ROADMAP wins, and the disagreement belongs in
  [Inconsistencies found](#inconsistencies-found) until the lead fixes one or the other.
- **The visual map** is [`docs/design/h2-plan-map.html`](design/h2-plan-map.html). Serve the repo
  with `python3 tools/serve_labs.py` and open it. Click a part to see its rows (read live from
  ROADMAP.md), its decisions, and what it unblocks. `?check=1` runs its self-check; check 4
  compares the map with the parts table below, and check 5 proves the critical path. (No gate in
  `./verify` runs these five checks; `lab_load_check` only loads the page. Run them served.)
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
| "Bend laws system ported over from H1 (particularly the inertial mass-spring; my own proprietary concept)." | `bend` | B397, ADR-096, B278, B57, B504, ADR-232 |
| "… pitch quantization interface from H1 will be ported over and made more generally available to other FX and devices …" | `quant`, `arps`, `seqgen` | B391, B88, ADR-190 A5; B554, ADR-290 (quantize on every pitch route); B539, ADR-264 (Arp) |
| "All relevant parameters will be made available to modulation." | `seams`, `modmatrix` | B275 (the manifest declares every parameter's mod limits) |
| "Modulators labs + envelopes labs will deliver the mod page interface." | `modulators` | B208, B226, B370, B377 |
| "All relevant parameters will be made available to morph." | `seams`, `morph` | B275 (the manifest declares every parameter's morph class) |
| "Morph page labs will deliver the morph." | `morph` | B211, B269, B424 |
| "Eventual inclusion of Tonality …" | `tonality`, `microtuning`, `mts` | B390, B431 |
| "History tree page concept will be ported over from H1 and rigorously tested for edge cases …" | `history` | B389 |
| "All FX modules will be designed and built. Their preset systems will be roped into the larger preset system." | `rack`, `maw`, `sluice`, `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `ott`, `kchorus`, `presets` | B393, ADR-190 A7–A8, B450, B395; ECHO a sibling (ADR-249, B526) |
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

Added since by ruling (2026-10-20): the horde FX device (`fxdevice`), after 1.0, with every
decision from now on stating its consequence for it (ADR-254, B528).

## The parts

"Needs first" lists completion prerequisites: a part cannot be done before them. In the map, an
edge that cites a row or ADR is solid, and an edge that is this plan's own inference is dashed.
The basis of every edge is in the map (hover an edge, or click a part).

**Changes in this refresh (2026-10-27).** One part added: `fxdevice`, the horde FX device,
post-1.0 (ADR-254, B528). Five statuses moved, each on its record: `sub` and `noise` planned → in
progress (designed by the workshop B514 and ADR-255, written as spec round 26, ADR-258, and built
in the engine's accepted stage 6, ADR-268); `echo` planned → in progress (the sibling Echo spun up
on 2026-10-19 and filed its notice, `integrations/echo/notice-spinup.md`, B526, ADR-249); `arps`
planned → in progress (the sibling Arp spun up 2026-10-21 and is at its admission, M3, B539,
ADR-282); `gui3` planned → in progress (the house design system is published and being revised,
B531, ADR-259; its labs are built, B544–B549, B555–B557). One prerequisite added: `arps` needs
`shell` ("horde's shell hosts it", B539). No part id is renamed or retired. Rows added: `engine2`
B514, B516–B525, B533, B534, B552; `quant` B554; `arps` B539, B543, B550, B553, B558;
`modulators` B540, B541; `modmatrix` B532; `morph` B557; `history` B537; `sub` B514; `noise`
B514, B515; `rack` B508, B511, B513, B527, B529, B530, B538; `maw` B508; `sluice` B510; `reverb`
B513; `perf` B551; `armor` B505, B507, B535; `gui3` B531, B532, B536, B544–B549, B555–B557;
`sampler` B515; `seqgen` B539, B559. The earlier refreshes' changes are in their traces
(`traces/2026-10-09-h2-plan-refresh.md`, `traces/2026-10-17-h2-plan-refresh.md` and `-r2.md`).

<!-- parts-table:start -->
| Part | id | Status | Governing rows | Needs first | 1.0 |
|---|---|---|---|---|---|
| The new engine (clean room) | `engine2` | in progress | B494, B493, B497, B498, B500, B502, B514, B516, B517, B518, B519, B520, B521, B522, B523, B524, B525, B533, B534, B552 | `audit` | in |
| Composed engine | `engine` | superseded | B327, B298, B252, B335, B310, B382, B355, B458 | — | superseded |
| Engine audit + legacy roundup | `audit` | ruled | B376, B386, B416, B420, B312 | — | in |
| Edge correction | `edges` | superseded | B383, B380, B378, B491 | — | superseded |
| Clean C++ port (phase 1b) | `port` | superseded | B385, B405, B404, B406, B379, B332, B378 | — | superseded |
| Seam contracts + readiness gate | `seams` | ruled | B275, B276, B277, B339 | `audit` | in |
| Bend laws + glide | `bend` | planned | B397, B278, B57, B32, B504 | `engine2`, `seams` | in |
| Shared pitch quantizer | `quant` | planned | B391, B88, B260, B554 | `bend` | in |
| MTS-ESP client | `mts` | ruled | B431, B390 | `quant` | in |
| MPE, aftertouch + MIDI basics | `mpe` | planned | B388, B431, B429 | `bend`, `modmatrix`, `engine2` | in |
| Simple arpeggiator (the sibling Arp) | `arps` | in progress | B391, B431, B539, B543, B550, B553, B558 | `quant`, `shell` | in |
| Modulators + envelopes | `modulators` | in progress | B208, B226, B366, B370, B377, B253, B259, B264, B267, B126, B540, B541 | `engine2` | in |
| Mod matrix | `modmatrix` | in progress | B392, B207, B57, B270, B282, B532 | `modulators`, `seams`, `audit` | in |
| Modulator morph | `modmorph` | planned | B396 | `modmatrix`, `morph` | in |
| Morph: editor, laws, quantum | `morph` | in progress | B211, B235, B268, B269, B272, B424, B240, B308, B557 | `seams` | in |
| Macros + intent bus | `intent` | ruled | B443, B442, B170, B270, B354, B453 | `morph`, `modmatrix` | in |
| FX algorithm morph | `fxmorph` | in progress | B265, B394, B266, B352 | `morph`, `rack`, `sluice` | in |
| Patch model → state schema | `patch` | in progress | B263, B308, B258 | `shell`, `morph`, `modmatrix`, `mix` | in |
| History tree | `history` | planned | B389, B84, B119, B122, B186, B222, B193, B537 | `shell`, `patch`, `fxmorph`, `intent` | in |
| The new Sub | `sub` | in progress | B399, B327, B278, B282, B514 | `seams`, `engine2` | in |
| Noise oscillator | `noise` | in progress | B409, B327, B514, B515 | `seams`, `engine2` | in |
| Filters | `filters` | in progress | B274, B287, B288, B289, B290, B303 | `seams`, `engine2` | in |
| True stereo | `stereo` | planned | B408, B414 | `engine2`, `filters` | in |
| Mixer + routing | `mix` | in progress | B402, B225, B435, B23, B258, B263, B499, B503 | `sub`, `noise`, `filters` | in |
| Master strip + limiter | `master` | ruled | B438, B402, B435 | `mix` | in |
| FX rack + slot contract | `rack` | planned | B450, B439, B50, B281, B262, B393, B435, B429, B463, B508, B511, B513, B527, B529, B530, B538 | `mix`, `seams` | in |
| Shriek (formerly MAW) | `maw` | in progress | B318, B373, B393, B433, B508 | `rack` | in |
| Sluice | `sluice` | in progress | B328, B321, B329, B337, B347, B353, B361, B369, B436, B273, B359, B393, B510 | `rack` | in |
| Scape (reverb) | `reverb` | in progress | B421, B152, B210, B393, B513 | `rack` | in |
| ECHO (delay, the sibling Echo) | `echo` | in progress | B425, B526, B386 | `rack` | in |
| Bulwark compressor | `bulwark` | in progress | B421, B234, B423, B450, B463 | `rack` | in |
| EQ | `eq` | planned | B234, B210, B336, B579 (sibling, ADR-313) | `rack` | in |
| FX filter | `fxfilter` | planned | B210, B288, B336, B580 (Weir, ADR-313) | `rack` | in |
| Drive | `drive` | planned | B210, B336, B579 (sibling, ADR-313) | `rack` | in |
| Bulwark multiband + ATM (was OTT) | `ott` | planned | B423, B400, B393 | `bulwark` | conditional |
| Kuramoto chorus | `kchorus` | post-1.0 | B401, B393, ADR-313 | `release`, `rack` | out |
| horde 2 plugin shell | `shell` | ruled | B398, B428, B429, B449, B305, B308, B306, B458 | `seams`, `engine2` | in |
| Legacy freeze tag | `freeze` | built | B255, B308, B305, B456 | — | in |
| Testing apparatus | `testing` | in progress | B316, B324, B340, B344, B350, B381, B186, B457, B490 | `engine2` | in |
| Performance | `perf` | in progress | B441, B445, B323, B372, B375, B357, B262, B488, B551 | `engine2` | in |
| Blind-Spot Armor + security gates | `armor` | in progress | B448, B446, B430, B455, B457, B505, B507, B535 | — | in |
| Conformance, distribution + legal | `distrib` | planned | B427, B447, B412, B433, B471, B472 | `shell` | in |
| GUI 3 + the screens | `gui3` | in progress | B302, B449, B432, B271, B322, B336, B261, B308, B531, B532, B536, B544, B545, B546, B547, B548, B549, B555, B556, B557 | `shell`, `fxmorph`, `intent`, `modmorph`, `mix`, `maw`, `sluice` | in |
| Presets + factory library | `presets` | planned | B395, B257, B261, B312, B349, B453 | `history`, `testing`, `intent`, `maw`, `sluice`, `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `gui3` | in |
| User manual | `manual` | in progress | B451, B413 | `gui3` | open |
| Random page (basic) | `random` | planned | B578, ADR-312 | `patch`, `morph`, `modmatrix`, `rack` | in |
| 1.0 — the stability line | `release` | planned | B305, B327, B440 | `presets`, `gui3`, `perf`, `freeze`, `quant`, `sub`, `noise`, `stereo`, `master`, `mpe`, `mts`, `arps`, `distrib`, `armor`, `random` | in |
| Tonality integration | `tonality` | post-1.0 | B390 | `release`, `quant` | out |
| Microtuning | `microtuning` | post-1.0 | B390 | `tonality` | out |
| Granular module | `granular` | post-1.0 | B393 | `release`, `rack` | out |
| Sampler | `sampler` | post-1.0 | B327, B330, B515 | `release`, `noise` | out |
| Complex arps, step sequencers, generators | `seqgen` | post-1.0 | B391, B539, B559 | `release`, `arps` | out |
| Vintage-textures FX module | `vintage` | post-1.0 | B444 | `release`, `rack` | out |
| Standalone application | `standalone` | post-1.0 | B411 | `release` | out |
| horde FX device | `fxdevice` | post-1.0 | B528 | `release`, `rack` | out |
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
  structure declaration for loupe's diagram), B502 (the two clean-room repositories), B514 (the
  sub and noise workshop before stage 6), B516–B525 (the clean-room independence audit's ten
  changes, ADR-247: the rulings log, the sealed hold-out, builder-origin tags, relay hygiene, the
  gate-relaxation ledger, BL-12's map, §10.6, the differential report, inline reasons, the
  filesystem separation), B533 (the off-machine copies), B534 (spec provenance), B552 (the edge
  laws at the bank stage). ADR-223 to ADR-232, ADR-222 (in the ED rows); the spec rounds since,
  14 to 42 (ADR-237 to ADR-289, the engine-side ones among them); ADR-247 (keep the gate by
  default).
- Status: **in progress, at the bank stage** (ADR-284, 2026-10-26). Done before this refresh: the
  rebuild audit (B493), the spec (ratified by #1097), the reference renders (B497, DONE 2026-10-17,
  with a 48 kHz set), the structure declaration (B500), the two repositories (B502). Since: the
  stages accepted one by one, each on the lead's rerun from a clean export (B494): stage 4 (the
  edges), stage 5 (the blades, ADR-253), stage 6 (the sub and noise, ADR-268, after the workshop
  B514 and spec rounds 26–28), ED-7 as a stage 5 amendment (ADR-271), stage 7 (modulation, morph
  and state, ADR-279), stage 8 (WebAssembly on Emscripten 6.0.13, ADR-280, ADR-283). The human's
  audit of the clean room's independence (ADR-247) added B516–B525: the sealed hold-out (B517)
  first ran at stage 7; "it worked" was written down before the bank stage as spec §10.6 (B522,
  ADR-284). The bank stage's rounds so far: 39 (ADR-285), 40 (ADR-286), 41 (ADR-287) and 42, the
  combined edge law (ADR-289, B552, DONE 2026-10-27), which the builder rebuilds the band-limiting
  to next. The gate-relaxation ledger's retroactive rulings are in (ADR-288); B520 stays open on
  the test author's owed engine-free checks. The differential report tool is built (B523,
  `tools/engine2_differential.mjs`) and waits for the new engine's bank renders.
- Acceptance (§10.6, ADR-284 item 4): the invariant suite, the hold-out resolved (its QS-9
  four-hour soak and OU-2 run before the bank stage is accepted, B517; the lead runs them,
  ADR-284), the capped A/B listening batch with no flag (ranked by B523), and the CPU in an
  accepted band (≤ 15 % passes, ≤ 29 % accepted with a follow-up, against B488); then the human's
  word. Size is reported, never a gate. Then it replaces the composed engine (B494 (4)).
- Needs first: `audit` (BD-8 keeps the parameter table open until spec Q31, the audit's 19
  deferred rows, is ruled).
- Open: the spec's questions still open (§11.1, §11.2; this refresh did not recount them against
  rounds 14–42); two of the three gaps the structure declaration exposed (B500): where the
  oscillators return to the host rate, and where IF-1's per-note volume and pan act, with IF-1's
  load readouts' consumer (ADR-232, RATIFIED by the merge of #1117, named the pitch system as the
  lifecycle's consumer); a gap found in accepted stage 7, PA-2's per-voice offsets, built at the
  bank stage (B494, 2026-10-26).

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
ADR-189's D1–D3 defaults, superseded by the new engine's spec (ADR-237 item 10); law 3's tempo grid is cut from 1.0 in Q31's
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
- Governing: B397, B278, B57, B32, B504. ADR-096, ADR-097, ADR-102, ADR-103, ADR-180, ADR-225,
  ADR-226, ADR-232 (RATIFIED 2026-10-17 by the merge of #1117), ADR-237 item 9, ADR-290.
- Status: **planned**. Built in legacy; B397 covers the port. The legacy roundup adds the
  simulated bend wheel and graphs after the bend laws (B386 item 8). The new engine keeps the
  pitch system upstream and reserves its contract (RC-3, ADR-226 item 7): it provides the per-note
  pitch input (IF-2), the published sounding pitch (ADR-225 item 7) and gravity's place, and never
  glides, bends or quantizes itself; glide and bend are spec Q6 and Q7, ruled during the build.
  **B504** (PLANNED 2026-10-17), the glide system in horde 2 (the human: "the glide system
  (particularly inertial spring mode with scale quantization) is a big part of what excites me
  about this synth"), carries forward poly glide, the four travel laws with the spring, scale
  quantization as a modifier, Glide From and the chord case legacy left open, after the engine's
  stage 2; **ADR-232** (RATIFIED) makes room for it in the engine (a pitch applies from a note's
  first sample; the pitch system receives the voice lifecycle). Old patches' glide keys are kept
  and written back unchanged until the pitch system exists (ADR-237 item 9). Scale quantize is
  also an option on every modulation route into a pitch (ADR-290, B554; the `quant` part).
- Needs first: `engine2` (RC-3); `seams`.
- Open: do the bank's 22 poly presets that store a glide time glide by default? Still deferred by
  the human ("happy to save poly glide for another day"); B504 holds it.

**The shared pitch quantizer** (`quant`). Legacy's scale quantize becomes a service other FX and
devices can call, with room for Tonality (B390). **In** for 1.0 (ADR-190 A5).
- Governing: B391, B88, B260, B554. ADR-106, ADR-158, ADR-093, ADR-190, ADR-290. Status:
  **planned**. The MIDI sidechain that sets the scale is after 1.0 (ADR-237 item 8). **New
  2026-10-27 (ADR-290, B554):** every modulation route into a pitch carries a Quantize toggle;
  when on, the destination's resulting pitch snaps to the global scale with the glide quantizer's
  hysteresis and minimum step time. Built with the pitch system (RC-3, B504).
- Needs first: `bend` (the quantizer is anchored on the bend lane, ADR-106).
- Open: mixed quantized and unquantized routes into one pitch (the lead's reading is that any
  route's toggle snaps it; for the human with the lab, B554).

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
  (ADR-226 item 4; aftertouch's destination is spec X21). Once Arp's streams are built, the
  sustain pedal holds `direct` voices only (ADR-282 item 1, deferred behaviour in RC-6.7).
- Needs first: `bend`, `modmatrix`, `engine2`.
- Open: the MPE design (B388); which expressions reach the swarm and the blades is spec Q2.

**Simple arpeggiator** (`arps`). A simple arpeggiator consuming the shared quantizer, built by
the sibling project **Arp** (ADR-264, B539), the home of note generation, which also owns `seqgen`
after 1.0. **In** for 1.0 (ADR-190 A5). "Arp" is a codename until B433's clearance (B539).
- Governing: B391, B431, B539, B543, B550, B553, B558. ADR-190, ADR-264, ADR-267 A1, ADR-269,
  ADR-270, ADR-272, ADR-282, ADR-291, ADR-293, ADR-294.
- Status: **in progress** (was planned). Arp spun up on 2026-10-21 (its notice, seq 1) and opened
  its M3, admission to horde (`integrations/arp/notice-m3.md`, seq 7). Ruled since: its 1.0 bar is
  `docs/proposals/note-processor-1.0-bar.md`, horde copies Arp's core in at Arp's V1 with its
  commit recorded, and its CPU sits in the modulation slice, 2 % (ADR-269); notes reach chosen
  sources through a per-source stream selector of three toggles, Direct, Chord and Arp, reserved
  in the spec now (rounds 30 and 31) and built with Arp, aiming for 1.0 as a recorded freeze
  exception (ADR-270, ADR-272, B543); its admission questions (ADR-282: the pedal holds `direct`
  voices only; triggers may run with no keys held; the user pattern rides per corner in Arp's
  preset; Arp authors its factory presets); the note-processor preset format is written (ADR-294,
  B550, `docs/proposals/note-processor-preset-format.md`). horde builds Arp's face and its undo
  (ADR-282, Q-H9): the Arp face lab (B553), round 2 with Order and Pattern pickers and a pattern
  visualizer (B558, ADR-291 item 4). Two arps are after 1.0 (`seqgen`, ADR-293).
- Needs first: `quant` (B391); `shell` (B539: horde's shell hosts it).
- Open: the human's ruling on the face lab (B553, B558); Arp's presets loading and round-tripping
  in the format (B550; Arp's, not the human's).

### Modulation

**Modulators and envelopes** (`modulators`). The modulator lab (LFOs, the Kuro-LFO, ORBITAL, MIDI
trackers, note-on randoms, the dry-signal follower, coherence R as a source) and the envelope
hierarchy (ENV 1 global, shapeable stage curves). Together they deliver the mod page.
- Governing: B208, B226, B366, B370, B377, B253, B259, B264, B267, B126, B540, B541. ADR-165,
  ADR-161, ADR-162, ADR-224, ADR-226, ADR-255, ADR-266, ADR-267.
- Status: **in progress**. The labs are built. The new engine reserves the modulator bank's
  contract (RC-4): per-voice offsets by note id at a control rate of about 1,500 a second, ramped,
  and at audio rate on named destinations (ADR-224 items 4, 5; ADR-225 item 2). Envelope timing is
  ruled: a stage's time is exactly when it ends, under one law for ENV 1 and the bank's envelopes
  (ADR-226 item 10). The default curves are spec Q31. Ruled since 2026-10-17: no source has an
  envelope of its own; each source's level follows ENV 1 or one of ENV 2–4 from the bank, chosen
  in the source slot (ADR-255 item 5); the four labs' open design decisions (ADR-266: sample and
  hold as an LFO shape and a mode on ORBITAL's mappings, retrigger per card); a 2-D random source
  for XY pads, Walk, Jump and Scatter, to prototype first (ADR-266 item 8, B540); a trigger timer
  and hosted modules' declared actions (ADR-267, B541), triggers being performance events that
  never make history (ADR-267 A1). Triggered switches and iterators are after 1.0 (ADR-267 A2,
  B542).
- Needs first: `engine2` (RC-4).
- Open: linked note-on randoms (B264); ORBITAL velocity, back to a lab round (ADR-235 item 4,
  B267); the onset-scatter recommendation (B370; where it lives is spec Q13); the 2-D random
  source's modes after listening (B540).

**The mod matrix** (`modmatrix`). Routes, depths, per-route polarity, mod-on-mod, per-corner
modulation ranges. The spring displacement (B57) joins as a source.
- Governing: B392 (a near-term priority), B207 (the lab, built), B57, B270, B282, B532. ADR-168,
  ADR-141, ADR-136, ADR-261, ADR-266, ADR-290.
- Status: **in progress**. The lab is built. Ruled since 2026-10-17 (ADR-266): the page is a grid
  plus trace lanes with compact wires; polarity has four states, uni, bi, inverse and Control
  (depth stays a magnitude, superseding ADR-168's signed depth); a hosted module's macros show
  read-only; a silent corner's mappings show greyed when corner-level mappings arrive. Map mode is
  the matrix's fast write path (ADR-261, B532 (c)); a pitch route carries a Quantize toggle
  (ADR-290).
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
- Governing: B211, B235, B268, B269, B272, B424, B240, B308 (H3), B557. ADR-104, ADR-109,
  ADR-185, ADR-186 §5(g), ADR-190, ADR-262, ADR-266, ADR-277, ADR-291.
- Status: **in progress**. Editor lab rounds are built; stepped morph glide is designed in the
  morph editor lab and lands with the morph-glide port (B424). Ruled since 2026-10-17: grid edits
  are live, STEPPED takes a rate plus sync, and cohesion is one control for the whole field, with
  per-group overrides perhaps later (ADR-266 items 9–11); corners show their presets' real names,
  with a per-corner lock (it refuses edits) and revert (ADR-262), saved as ADR-277 rules (the pin
  and the lock are session state with no undo, a revert is one undo step); a per-parameter morph
  mode menu, Follow, Quantum or Exempt, as patch state (ADR-291 item 3, B557; its lab is the page
  chrome lab, `docs/design/page-chrome-lab.html`).
- Needs first: `seams`.
- Open: B269's remaining items (boundary editing in BLEND/STEPPED, and locks; per-group cohesion
  is ruled as one control by ADR-266 item 11, and a per-parameter Exempt by ADR-291 item 3, the
  lead's reading of how far they answer B269); the counter-based draw ADR, owed before the first
  horde 2 patch is saved (B308 H3); the page chrome lab's open questions on Exempt and Quantum
  (B557, its trace).

**Macros and the intent bus** (`intent`). Eight macro slots: four fixed intents (Tone, Space,
Time, Motion) that mean the same in every global preset, and four named slots that flip as units
across the morph grid (ADR-192). The intent bus lives on horde's global mod matrix and binds
nothing automatically; a module may offer up to eight ordered, labelled macros, with no role
vocabulary (ADR-169 A4) and binding curves `lin`, `exp`, `log` (ADR-169 A3).
- Governing: B443, B442, B170, B270, B354. ADR-192, ADR-152, ADR-176, ADR-169, ADR-188 A1,
  ADR-251, ADR-266.
- Status: **ruled** (was planned). It is built in legacy through phase 2; phase 3 and the routing
  half are owed. Since 2026-10-17: a hosted module's macros may follow the modify law, marked per
  preset (ADR-251, B527, in the rack's slot), and show read-only in horde's matrix (ADR-266 item 3).
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
- Governing: B263, B308, B258. ADR-186, ADR-277, ADR-291, ADR-294. Status: **in progress**,
  ratification pending. Since 2026-10-17: which new user state is preset state and which session
  state is ruled (ADR-277); the per-parameter morph mode is patch state (ADR-291 item 3); a hosted
  module's blobs ride beside its cascade preset in horde's corner record (ADR-294).
- Needs first: `shell`; `morph`, `modmatrix`, `mix`.
- Open: ratify the patch model.

**History tree** (`history`). H1's history, re-earned on horde 2: a shell-owned snapshot tree,
gesture-level nodes, fork on restore. It must survive preset changes, morphing, modulation,
mapping changes, signal-flow changes and FX module edits, with B222's defects as regression rows.
- Governing: B389 (high priority), B84, B119, B122, B186, B222, B193, B537. ADR-160, ADR-229,
  ADR-235, ADR-267, ADR-277.
- Status: **planned**. The new engine reserves what history needs (RC-11, ADR-229 A1): no hidden
  user state, a real-time-safe snapshot and restore, the host's gestures passed through so one
  drag is one undo step, and a restore that re-triggers no note. The undo stack itself is the
  shell's and the GUI's. Ruled since: the scope (ADR-235 item 2: modulated values in motion are
  not history; mapping changes and loading a preset into a module are; hand edits versus DAW
  automation to be measured per host); triggers never make a history node (ADR-267 A1); the
  corner pin, the lock, revert and map-mode mappings (ADR-277, B537 DONE).
- Needs first, to be DONE: `shell`, `patch`, `fxmorph`, `intent`. The PORT can start as soon as
  horde 2 has state ([Order of work](#order-of-work) A3).
- Open: nothing new for the human; the per-host measurement is owed when the system is built
  (ADR-235 item 2).

### Sources and routing

**The new Sub** (`sub`). A new sub oscillator, workshopped. The legacy Sub is not lifted as it is.
- Governing: B399 (the workshop row, still OPEN — planned), B327, B278, B282, B514 (the sub and
  noise workshop, DONE 2026-10-21). ADR-178, ADR-190, ADR-224, ADR-225, ADR-246, ADR-255, ADR-258,
  ADR-268.
- Status: **in progress** (was planned). The workshop ran as B514 (a browser audition lab, the
  human's rulings by poll and from the lab, 2026-10-18 to 10-20), in 1.0 as a recorded exception
  to the freeze (ADR-246 item 4). Ruled (ADR-255, B514): a mono sub following the lowest held note
  by default (also highest, newest, and a per-note mode), its glide from the pitch system; Coarse
  ±48 st; the classic ±1 pulse, its DC removed exactly after the drive (ADR-255 item 9, its reason
  corrected by A1); no noise wave; the bump with two extra harmonics, 2nd to 9th; three drive
  characters; its level following ENV 1 or ENV 2–4 from the source slot; 'harmonics' and 'ratio'
  pitch modes after 1.0, with a key reserved. Written as spec rounds 26–28 (ADR-258, ADR-260,
  ADR-263) and built in the engine's stage 6, accepted at `bc84ae4` (ADR-268). It is done when the
  new engine replaces the current one.
- Needs first: `seams`; `engine2` (the one published per-voice pitch B278 asked for is now the
  engine's, ADR-225 item 7).
- Open: nothing for the human. B399's first question, what the human dislikes in the legacy Sub,
  was never answered as such; the workshop ruled the sub instead (see [Inconsistencies
  found](#inconsistencies-found)).

**Noise oscillator** (`noise`). A plain noise oscillator, a 1.0 source separate from the
post-1.0 sampler (ADR-190 A6, answering B409's question).
- Governing: B409, B327, B514 (its workshop, by ear), B515 (samples after 1.0, the reservation
  in 1.0). ADR-190, ADR-224, ADR-246, ADR-255, ADR-258, ADR-268. B452 still tracks its design lab.
- Status: **in progress** (was planned). Designed in B514's workshop and ruled (ADR-255): four
  sources (Clean, Crackle, S&H, LFSR), three optional stages in series (Resonator → Whisper →
  Wind), its own filter ahead of the shared filters, Stereo spread, a Pitch control (±48 st), its
  level on ENV 1 or ENV 2–4 from the source slot. Written as spec round 26 (ADR-258) and built in
  stage 6, accepted (ADR-268). It will also play samples, AFTER 1.0, with its parameter ids, a
  source mode and a sample slot reserved in 1.0 (ADR-246, B515; the `sampler` part).
- Needs first: `seams` (it follows every global unless its manifest opts out, B275 (b));
  `engine2` (it is a source in the voice, ADR-224 item 1).
- Open: nothing for the human. B515's 1.0 part owes a round-trip test with the reserved keys
  absent and present.

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
  ADR-088, ADR-175, ADR-230, ADR-231, ADR-235, ADR-272.
- Status: **in progress** (B225's lab exists; B402 carries it into horde 2). Ruled 2026-10-16
  (ADR-230 and its A1): the make-up gain is a user control (default ×4, +12.04 dB, restoring
  ADR-221's 12 dB) between the engine's output and the rack's first slot, and the gain-staging
  view gives every stage a gain control, a meter, a peak hold and a clip lamp that latches until
  cleared (spec RC-2, RC-8). Per-preset make-up by loudness is ruled for the bank stage (ADR-231,
  B499); an adaptive make-up mode waits for its own lab (B503, after B402). The rack-input lamp
  latches an over like every other stage (ADR-235 item 3). The design system now carries the
  gain-staging view and the one-line chain view, whose gains are fixed offset bars under the line
  (B544, B545, B549; ADR-272 A1), awaiting the human's look; their ranges wait on B402.
- Needs first: `sub`, `noise`, `filters` (inferred: the strips are the sources, the routing
  places the filters).
- Open: the make-up's range and the faders' laws (B402); whether the adaptive mode ships (B503);
  corner FX buses (B258); per-source routing into the rack, since RC-5 hands it one mixed pair
  (B528's audit, an FX-device question for the human).

**Master strip and limiter** (`master`). One instance of Bulwark's limiter face, never a rack
slot, the last stage before the output. The mixer page's master strip has limiter on/off,
Ceiling, Release, a gain-reduction meter, the clip latch, and a cut-only Volume.
- Governing: B438 (approved 2026-10-04, "master only for 1.0"), B402, B435. ADR-195 (Ceiling
  default −1.0 dBFS), ADR-190, ADR-275, ADR-292.
- Status: **ruled**. Bulwark mirrored the −1.0 default; the thread closed 2026-10-09. With the
  make-up before the rack (ADR-230) and no peak guarantee in the engine (ADR-226 item 8, spec
  OU-1), this limiter is what holds the device's peaks. Since 2026-10-17: Bulwark made it a
  true-peak limiter (4× interpolated detection; held on Bulwark's material, measured, not
  proven), allowed up to 1.0 % of a min-spec core, the master strip's slice growing to 1.5 %, and
  its latency growing by 6 samples (ADR-275). It is always in horde's output path, off only by the
  player's explicit choice on the master strip, with the clip latch reporting overs while it is
  off, and Bulwark's ATM may rely on it (ADR-292 item 1).
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
- Governing: B450, B439, B50, B281, B262, B393, B435, B429, B463, B508, B511, B513, B527, B529,
  B530, B538. ADR-172, ADR-175, ADR-128, ADR-190, ADR-193, ADR-195, ADR-201, ADR-202, ADR-203,
  ADR-218, ADR-234, ADR-236, ADR-237, ADR-242, ADR-251, ADR-254, ADR-257, ADR-267, ADR-294.
- Status: **planned**. horde 2's rack is unbuilt. The hosted-module rack-slot contract is
  **RATIFIED** (`docs/proposals/rack-slot-contract.md`, ADR-201, 2026-10-09); Q3 and Q9 are
  provisional until measured. It takes FOUNDATIONS' FX-operator ABI as the code-level interface.
  Hosted modules' presets (ADR-202) and the module bar's readings (ADR-203) are ruled. Its 3 dB
  click ceiling is **suspended** as pass/fail until the first listening batch sets one that does
  not depend on the probe's frequency or the sample rate (ADR-218, answering Bulwark's readiness
  notice, B463). The engine's side is reserved: one stereo pair at zero latency, after the
  make-up gain (spec RC-5).
- Ruled since 2026-10-17, as contract rows built with the horde 2 rack: the rack runs at the host
  rate, each module oversampling as it needs (ADR-237 item 1, §12.3); a module's factory presets
  may default to an oversampled mode within 4 % and a declared, activate-fixed latency is allowed
  (ADR-237 items 2, 3, B508); seeded jumps and group delay (ADR-234, §12.2); an optional
  morph-position call (RS-4.10, ADR-242, B513, reviewed by Scape); the modify macro law (ADR-251,
  B527, adopted by Shriek); a declared fixed row (RS-5.8) and a per-block note context conformed to
  B92 (RS-4.11; ADR-257 and its A1, A2; B529, B530); declared actions (RS-4.12, A23, ADR-267); a
  module's blobs beside its cascade preset (RS §6.2 amended, ADR-294). The module bar gains the
  automated-parameter budget, 4 % (ADR-236 item 4, ADR-237 item 6, B511), and memory is a MUST
  (ADR-236 item 6). Every rack decision states its FX-device consequence (ADR-254).
- Needs first: `mix` (inferred); `seams` (B281).
- Open: measure Q3 (the click ceiling, now with ADR-218's suspension) and Q9 (the state budget);
  build the admission test with the shell (B398); reconcile §12.5 with RS-5.6 and amend
  SPEC-MODULE-MACROS's base line, which is protected (B538); B530's overflow rule for the capped
  event list. The module cutoff is set: Friday 2026-12-11 (ADR-236).

**Shriek, formerly MAW** (`maw`). The three-stage saturator, FX-C, a hosted sibling module.
Renamed by the human on 2026-10-04 (B433); the part id stays `maw`. B318, B373, B393, B433,
B508. ADR-170, ADR-092, ADR-190, ADR-237, ADR-251, ADR-257, ADR-292. **In progress.** Needs
`rack`. Its level contract is answered (B435). Its three questions are ruled: the rack's host
rate, oversampled factory defaults within 4 %, and its 2× latency declared and fixed at activate
(ADR-237 items 1–3, B508). It adopted the modify macro law on all 13 factory presets (B527) and
declared four fixed controls (ADR-257). Its lab syncs in S4 at its turn, when its scope list
freezes, and its own prototype replaces horde's stale `docs/design/maw-horde-lab.html`, with its
source commit recorded (ADR-292 items 2, 3). Open: FX-C as a fixed post-stage or a rack slot (the
new engine builds a rack slot and no engine insert until spec Q23 is ruled, after the build), and
B318's other items.

**Sluice** (`sluice`). The morphable FX network, a sibling project. One patch per module XY;
different patches at horde's morph corners (ADR-188). B328, B321, B329, B337, B347, B353, B361,
B369, B436, B273, B359, B393, B510. ADR-188, ADR-166, ADR-193, ADR-236, ADR-237, ADR-267.
**In progress** (labs). Needs `rack`.
- Inside horde (ADR-236 item 7, ADR-237 item 7, B510): Sluice's own XY runs in its slot, visible,
  not hidden (ADR-166's hosted-XY reading withdrawn), and its library (patches, presets,
  snapshots, XY) is browsable and loadable there, including the standalone Sluice library,
  read-only (ADR-202 item 3 amended). How "a little harder to find" it is waits for the slot UI.
  A notice asks it to declare its actions, such as clear (ADR-267).
- Consumption is **ruled** on Sluice's side (Sluice D-096, recorded in B328 on 2026-10-04): horde
  takes Sluice's code into its own tree at Sluice V1, when Sluice goes public, with no tag and no
  lock file. The private-optional-dependency proposal is superseded.
- It answered the engine-rebuild-audit brief (B494): its human ruled a targeted clean-up, not a
  rebuild (Sluice D-139). Its question, which side of the slots the make-up sits on, is ruled by
  ADR-230: before them, so Sluice sees horde's earlier level.
- Open: the lab's tails (B369); ADR-193 D5 plans a per-module tail cap.

**Scape, the reverb** (`reverb`). Built as the sibling project Scape (B421, spun up 2026-10-02).
B421, B152, B210, B393, B513. ADR-177, ADR-190, ADR-198, ADR-242. **In progress.** Needs
`rack`. Its audit chose a targeted clean-up, not a rebuild (B508). Its brief asked for the
optional morph-position call, ruled as RS-4.10, whose shape it reviewed (ADR-242, B513). It
measures 10.3 % with Decay automated, against the 4 % automated-parameter budget (B511).

**ECHO** (`echo`). horde's standard delay, built as the sibling project **Echo** and hosted in
the rack (ADR-249, B526, 2026-10-19, reversing B425's "stays in horde"): sync or time, L/R
offsets, ping-pong, mid/side or standard, feedback, delay modulation modes including repitch, and
input, delay-line and output filters (the human's scope, ADR-249). B425, B526, B386. ADR-190,
ADR-142, ADR-201, ADR-249. **In progress** (was planned). Echo spun up on 2026-10-19 and built
its M1 and M2 before filing its notice (seq 1, ball none, landed 2026-10-23 in
`integrations/echo/`). "Echo" is a placeholder name until B433's clearance. Needs `rack`.

**Bulwark compressor** (`bulwark`). The compressor face of the sibling project Bulwark (formerly
Dynamite), hosted in the rack; its limiter face is `master`. B421, B234, B423, B450, B463.
ADR-190, ADR-195, ADR-198, ADR-218, ADR-234, ADR-275, ADR-292. **In progress**: its M1–M3 are
closed, and M4, hosting in horde, waits on the slot contract (B450). Its readiness notice is
answered (ADR-218, B463: crossfade its two detector switches; a declared tail may mean the time
until the instance is at rest). Its switch classes are ruled (ADR-234: `comp.scOn` a Device key;
seeded jumps admitted; group delay stated outside the residual). The engine-rebuild-audit brief
is with it (B494). Needs `rack`.

**EQ** (`eq`), **FX filter** (`fxfilter`) and **Drive** (`drive`). The three simple FX modules
(ADR-190 A7). EQ is the parametric EQ of FX lab round 2 (B234); comb and notch are filter types,
not modules (B288); drive is separate from Shriek so it can sit before and after another module.
B210, B234, B288, B336. ADR-190, ADR-172. **Planned**, each with **no row of its own**. Each
needs `rack`. One glass each (ADR-235 item 1); until their skins are chosen, each shows a
labelled stand-in screen in the design system (ADR-273). Open: the names, which the human picks
from the FX-screens workshop (ADR-237 item 12, B336).

**Bulwark multiband and ATM, was OTT** (`ott`). Not a module of its own: a multiband mode of
Bulwark's compressor, with ATM ("above the maximum") as a factory preset (the human, 2026-10-04,
B423). B423, B400, B393. ADR-190, ADR-195 (band count is flip-class, never summed in parallel with
an unphased branch), ADR-292. **Planned**, **conditional**. Needs `bulwark`. Bulwark replaced
ATM's band with the OTT model's structure, whose hits can overshoot after quiet passages; ATM may
rely on horde's master limiter to catch them (ADR-292 item 1), and horde plans the slot's tail
handling for its new worst case (ADR-292).

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
  state), ADR-200, ADR-207, ADR-209, ADR-211, ADR-215, ADR-223, ADR-229, ADR-235, ADR-282.
- Status: **ruled**. The skeleton is not built. **Build first** (ADR-215): a playable preview, a
  thin plugin around the current C++ engine under its own identity ("horde 2 PREVIEW"), is BUILT
  and INSTALLED (B458, 2026-10-12), deliberately not the B398 skeleton and skipping ADR-186 §5's
  rules, which are owed before the first release. The skeleton's acceptance list is drafted
  (`docs/proposals/h2-shell-acceptance.md`, 42 rows); flush-to-zero (ADR-211), the latch-and-report
  output guard (ADR-209), the thread-handoff rules (ADR-200) and the host-boundary checks
  (ADR-207) are ruled for it. It will host the new engine (ADR-223), which reserves what a shell
  needs: a lock-free GUI readout snapshot (RC-8), append-only parameter ids with unknown keys kept
  (RC-9) and history support (RC-11). Its CLAP id is `com.mindlathe.horde` (ADR-235 item 5), and
  its AU manufacturer code `Mndl`, proposed there and ruled as the fleet code by autonomous
  (its response to brief hypersaw-017, seq 2, 2026-10-17; horde's records do not yet note it). It
  hosts Arp, teeing the player's input to the engine as `direct` before Arp (ADR-282, B539).
- Needs first: `seams`; `engine2` (was `port`).
- Open: which spec the rest of horde 2 (the shell, the rack, GUI 3) answers to (B308 M2); the
  engine's is `specs/SPEC-ENGINE.md` (ADR-224 item 8). Ratifying the shell acceptance list with
  B398 (ADR-209, package P17).

**Legacy freeze tag** (`freeze`). The human cuts the legacy tag; CI archives the universal signed
bundles; old projects depend on that archive (ADR-186 §2). Legacy sessions stay on the old system
(ADR-197).
- Governing: B255, B308 (H1, H5, M4), B305, B456. ADR-186, ADR-207, ADR-208, ADR-209. Status:
  **built** (2026-10-17). The tag `v0.9-legacy` was cut on main `57d61f9`; CI built, signed
  (ad-hoc) and drafted the release; the human published it. Its assets are the legacy plugin of
  record. A side-by-side test identity exists for trying legacy changes without replacing the
  installed plugin (B456). Needs first: nothing in this graph.
- Open: nothing. (The real-blob corpus of B308 H5 is skipped: ADR-209 item 11.)

**Testing apparatus** (`testing`). The patch-space gauntlet, the blind listening passes, the
aliasing metric, the Serum 2 reference test and the C++ quality suites.
- Governing: B316, B324, B340, B344, B350, B381, B186, B457, B490. ADR-187, ADR-209, ADR-227.
  Status: **in progress**; Serum stage 1 merged (#886); the engine harness and nightly runner
  (B457). For the new engine, the acceptance tests (SPEC-ENGINE §10.1, with §8's and §9's checks)
  are written from the spec alone by a different agent, stage by stage ahead of the build (B498;
  the test-author session runs, B502), calibrated on the reference renders (B497). The click
  detector is ratified at k 10 (ADR-227, B490). The independence audit's apparatus (ADR-247) is
  listed under `engine2`: the sealed hold-out (B517), the gate-relaxation ledger (B520, ADR-288)
  and the differential report against the legacy engine (B523, built, reported and never a gate).
- Needs first: `engine2` (B498; was `port`).
- Open: loading Serum 2 after stage 1 (B381); the aliasDb re-ruling (B350).

**Performance** (`perf`). C++ CPU against the B439 budget (sources ≤ 34 % of a min-spec core for
8 voices; CPU is Layer-E, never a gate, ADR-187 §8), the voice limit, oversampling as a quality
option, a WASM build for the labs, R2 and R3, FX cost.
- Governing: B441 (the CPU campaign), B445 (os 1 default, os 2 HQ), B323, B372, B375, B357, B262,
  B488, B551 (the AudioWorklet wrapper for the labs). ADR-187, ADR-191, ADR-209, ADR-220, ADR-225,
  ADR-280, ADR-283, ADR-284, ADR-289.
- Status: **in progress**. Specialised kernels landed bit-identical (B441 phase 2); the os 1
  default flip is built (#984), with the ADR-197 migration; the current engine may change its
  sound for speed when heard A/B (ADR-220, B488). The target is restated for the new engine (B488,
  B494): the heaviest bank patch at about 10.7 % of an M3 performance core per swarm at 8 voices;
  the rebuild audit estimates a clean engine at about 11.5 % (B493). Polyphony is ruled: up to 64
  voices preallocated, default 16, no CPU-adaptive limiting by default, the budget still measured
  at 8 (ADR-225 item 13). Since 2026-10-17: the WebAssembly toolchain is Emscripten 6.0.13
  (ADR-280 item 1), and the new engine's WebAssembly build is accepted (stage 8, ADR-283); the
  AudioWorklet wrapper that lets a lab play it is built with the labs, outside the clean room
  (ADR-283 item 2, B551). The new engine's CPU bands are ruled for cutover (ADR-284 item 2: ≤ 15 %
  passes, ≤ 29 % is accepted with a follow-up, over 29 % fails, against B488); round 42's combined
  edge law costs more and is measured at cutover (ADR-289).
- Needs first: `engine2` (was `port`, `edges`).
- Open: R2 and R3 (B357); the heavy-patch policy for the current engine (B441 phase 3); one CPU
  measurement on the human's Windows PC (ADR-209 item 10).

**Blind-Spot Armor and security gates** (`armor`). Every risk-register row is a named gate in
`./verify` or a visible hole on the dashboard; correctness and security share one catalogue.
Includes the runtime safety nets (B430).
- Governing: B448, B446, B430, B455, B457, B505, B507, B535. ADR-197, ADR-194, ADR-196, ADR-199,
  ADR-200, ADR-206, ADR-207, ADR-209 to ADR-215, ADR-233.
- Status: **in progress**. It has been law since 2026-10-08. Phase 1 (the retrofit) is complete.
  Phase 2 is ruled (ADR-209: engine now, shell at birth, legacy frozen) and its Wave 1 is built
  and merged (2026-10-11). Score on 2026-10-11: 0 of 12 risk rows fully gated, 12 partial, none
  missing; at ratification it was 0, 11 and 1 (soak). Since 2026-10-12 new armor work waits
  (ADR-215 item 4, "build first"): gate rows found since are filed and not chased (B470, B473,
  B492, B495, B496). Since 2026-10-17: CI's sanitizer jobs run in parallel and the thread job runs
  the real thread test, CI down from 43 to about 21–25 minutes, accepted by the human (ADR-233,
  B505 DONE); `./verify` runs store-touching oracles under a scratch home and brackets every `fast` and
  `full` run with a check of the real store's `logs/` (B507, ADR-237 item 11; built in `6abd916`); the armor checks see `engine2/` (B535, built, PR #1208). This refresh did not
  re-measure the score.
- Needs first: nothing.

**Conformance, distribution and legal** (`distrib`). What a release must clear outside the DSP:
pluginval, auval and clap-validator on the signed builds (B427), the Windows host test before any
Windows release (B447), signed installers, licensing and the update path (B412), trademark and
licence items (B433). Platforms: macOS and Windows x64 (B432).
- Governing: B427, B447, B412, B433, B471, B472. ADR-190, ADR-196, ADR-204, ADR-205, ADR-209,
  ADR-219, ADR-236. Status: **planned**. Needs first: `shell`. For 1.0, a module's name
  clearance is B433's self-run knockout screen, recorded privately, not legal clearance (ADR-236
  item 5); "Arp" and "Echo" are working names until it (B539, ADR-249).
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
- Governing: B302, B449, B432, B271, B322, B336, B261, B308, B531 (the house design system),
  B532 (the morph knob and map mode specs), B536, B544–B549 (design-system components), B555–B557
  (the page chrome). ADR-116, ADR-186, ADR-190, ADR-229, ADR-230, ADR-235, ADR-259, ADR-261,
  ADR-262, ADR-266, ADR-272, ADR-273, ADR-277, ADR-291.
- Status: **in progress** (was planned). The screens workshops are built; the settings lab (B261)
  has not started. The new engine publishes a lock-free readout snapshot for the GUI (RC-8) and the
  meter readings the gain-staging view draws (ADR-230 item 4). Since 2026-10-21, while the engine
  builds:
  - **The house design system** (B531), one for GUI 3, published from the Design System Artifact
    type and revised by version; its eight questions ruled (ADR-259: Archivo with Roboto Mono;
    980×720, resizable; corner glyphs always; the chassis carries corner colours; screens follow
    the working skins; an opt-in keyboard mode; a dark theme after a contrast audit; 28 px
    targets), then the human's notes (ADR-262: the Specimen, screen schemes, real corner names,
    per-corner lock and revert, the census ring) and the four labs' components (ADR-266). Its
    README is brought in line (B536, built). FX modules are cream with a small screen each
    (ADR-273, B548), cables in their own colours (ADR-273 A1).
  - **The interaction specs** (ADR-261, B532): the morph knob and map mode specs and their three
    prototypes, normative for interaction and protected; shape-coded pips; hotkeys only in
    keyboard mode. The new user state they add saves, undoes and branches as ADR-277 rules.
  - **Components built and awaiting the human's look:** the Scalpel lab's phase ring as the
    Kuramoto circle, and the gain-staging view (B544); the chain line and blend maps (B545); the
    chain line's gains, now fixed offset bars (B547, superseded by B549; ADR-272 A1).
  - **The `window.Horde` bundle:** the design system's components as one vendored bundle
    (`docs/design/arp-face/bundle.js`; its README: "Nothing is hand-edited; re-vendor instead"),
    which the Arp face lab (B553) and the page chrome lab load. No row of its own records it.
  - **The page chrome lab** (`docs/design/page-chrome-lab.html`, ADR-291 items 1–3): the morph
    pad docked on every page (B555), a per-page modulator bank read from the matrix's routes
    (B556), and the per-parameter morph mode menu, Follow, Quantum or Exempt (B557). Each row asks
    the human to rule on the lab.
- Needs first: `shell`, `fxmorph`, `intent`, `modmorph`, `mix`, `maw`, `sluice`.
- Open: the phase carpet and scale-selector picks, and readout taps RC-8 does not carry (B531);
  the looks owed on B544, B545 and B549; the page chrome lab's six questions (its trace,
  `traces/2026-10-27-b555-page-chrome-lab.md`): Exempt collapsing the corners or holding a value
  apart, Quantum on a structural parameter disabled or a no-op, Quantum's own boundaries, Exempt
  parameters taking modulation, the rail's width and whether it starts collapsed, and the bundle's
  drags writing no undo node.

**Presets and the factory library** (`presets`). Global presets that cover everything the synth
can do; saving one also ingests each of its corners; every FX module's presets roped in. The
preset format writes `os` explicitly or carries a format version (B395, ADR-197).
- Governing: B395, B257, B261, B312, B349, B453. ADR-186, ADR-167, ADR-197, ADR-202, ADR-169,
  ADR-226, ADR-231, ADR-251, ADR-282, ADR-294. Status: **planned**. The bank's levels are evened
  by loudness at the build's bank stage (ADR-231, spec OU-2); velocity and aftertouch are mapped
  by default in the factory presets (ADR-226 item 4). Since 2026-10-17: a hosted module's preset
  may mark the modify macro law (ADR-251); a module's blobs ride beside its cascade preset, and
  the note-processor format is written, in which Arp authors its own factory presets (ADR-294,
  ADR-282 item 4, B550).
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

**APPROVED 2026-10-27 (ADR-292 item 2, B331): B331's lab-sync order, S0 to S6, as the lead first
proposed it.** The human approved B331's own order, not the reordering this plan proposed on
2026-10-17 (which moved the Sub and filters to a later stage and promoted modulation and the FX
algorithm morph). Track B below is B331's order. Each module's scope list freezes at its lab sync
(ADR-190 A1); Shriek syncs in S4, at its turn (ADR-292 item 2).

Two orders, on purpose. The [parts table](#the-parts)'s edges are COMPLETION dependencies. The
stages below order the human's REVIEW sessions, and a lab can rule its laws before its
prerequisites are built. The FX algorithm morph, for example, ruled D1–D8 and built three lab
rounds long before the rack exists, and GUI 3's design system is being built now, while the
engine builds (B531, at the human's ask), long before S5.

**Track A — the C++ spine** (agent-paced; runs alongside the lab sessions):

| Step | Work | Parts |
|---|---|---|
| A1 | **The new engine** (ADR-223, B494): the composed engine rebuilt from a user-level spec in a clean room. DONE: the rebuild audit (B493), the spec (#1097), the reference renders (B497), the repositories (B502), and stages 1 to 8, each accepted on the lead's clean-export rerun (stage 5 ADR-253, stage 6 ADR-268, ED-7 ADR-271, stage 7 ADR-279, stage 8 on WebAssembly ADR-283). NOW: the bank stage (ADR-284): the builder rebuilds the band-limiting to spec round 42 (ADR-289), builds the bank translator and PA-2's per-voice offsets; the lead runs the hold-out's QS-9 soak and OU-2 (B517) and the differential report (B523). THEN: cutover on §10.6's four conditions and the human's word (ADR-284 item 4). Meanwhile the current engine plays in the preview (B458, ADR-215). | `engine2`, `testing`, `perf` |
| A2 | The shell skeleton (ADR-186 §5), after the preview that came first (ADR-215, B458): the new identity (`com.mindlathe.horde`, ADR-235 item 5); the manifest feeding a generated parameter lockfile (B308 H4), now the new engine's manifest (spec §5.1, append-only by RC-9), with B277 re-scoped to horde 2; the state schema version; the counter-based draw ADR (B308 H3). Frozen identities (B428), the wrapper boundary (B429) and the GUI rules R1–R11 (B449) land with it. It hosts Arp (B539). | `seams`, `shell` |
| A3 | History on the skeleton (B389), ported as soon as horde 2 has state, its acceptance grown as each feature lands; the engine's side is RC-11 (ADR-229 A1); its scope is ruled (ADR-235 item 2, ADR-267 A1, ADR-277). | `history` |
| A4 | The pitch seam: the bend laws and glide on the engine's per-note pitch input and published pitch (RC-3, ADR-225 item 7), with B504's glide system after the engine's stage 2 (ADR-232); the quantizer as a service, with Quantize on every pitch route (ADR-290, B554); MTS-ESP with its stated precedence, and MPE through CLAP per-note expressions; room for Tonality (B390). | `bend`, `quant`, `mts`, `mpe` |
| A5 | The gates 1.0 must pass: Blind-Spot Armor (ADR-197; phase 1 done, phase 2 ruled, ADR-209, with new armor work waiting while horde builds first, ADR-215 item 4), the conformance gates once a signed shell exists (B427), the GPL at 1.0 and the pre-commercial legal read before any paid build (ADR-219, B472). | `armor`, `distrib` |

**Track B — lab sync** (human-paced, one lab per review session; B331's order, approved by
ADR-292). A part B331 does not name is placed by this plan, and says so:

| Stage | Work (B331's, unless marked) | Parts |
|---|---|---|
| S0 | Triage. Done in substance: the navigator archives the dead-engine and superseded labs (B407). | `archived` |
| S1 | Engine: the SCALPEL interface, then the Sub workshop, then filters. **Done in substance but for filters:** B376 and B386 decided; the edge correction ruled (ADR-222); the SCALPEL interface became the new engine's ratified spec (ADR-223 to ADR-229); the Sub workshop ran as B514 with the noise oscillator (placed here by this plan, since B514 workshopped both) and is ruled (ADR-255) and built (stage 6, ADR-268). Left: the filters (B274, B287–B290, B303; spec X13) and the spec's open questions (§11.1, among them Q31). | `audit`, `engine2`, `sub`, `noise`, `filters` |
| S2 | Modulation and morph: the modulator lab, then the mod matrix lab (B392, with B57's spring source), then the morph editor with its laws and quantum (B424's stepped morph glide), then the patch model (B263, the state schema). Placed here by this plan: the modulator-morph workshop (B396, part of the mod lab) and the intent slots (ADR-192). Ruled so far: the four labs' design decisions (ADR-266). | `modulators`, `modmatrix`, `modmorph`, `morph`, `patch`, `intent` |
| S3 | Routing and mix: the mixer and routing lab with the master strip and the gain-staging view (B402, B438, ADR-230), then the adaptive make-up lab (B503). Placed here by this plan: the stereo lab (B408), the MPE design (B388) and the arpeggiator's hosting (Arp, B539). | `mix`, `master`, `stereo`, `mpe`, `arps` |
| S4 | FX: the FX chain morph, the FX page and design, Shriek (its own prototype replacing horde's copy, ADR-292 item 3), Sluice after B328, the rest (Scape, Bulwark, Echo, EQ, FX filter, drive; the conditionals by the module cutoff), then the screen aesthetic and logos. The rack-slot contract is ratified (B450, ADR-201). | `fxmorph`, `rack`, `maw`, `sluice`, `reverb`, `echo`, `bulwark`, `eq`, `fxfilter`, `drive`, `ott`, `kchorus` |
| S5 | Presentation, then GUI 3: settings, layout, compact and aesthetics consolidated. The settings lab (B261) first; B449's rules and B432's accessibility apply; the design system (B531) and the page chrome (B555–B557) are already under way. | `gui3` |
| S6 | Calibration and content: the listening pass, the random patch P4/P5, the legacy preset import (B312), the factory bank (B257, B395). Placed here by this plan: the Serum gauntlet (B381) and the manual's prose and screenshots (B451). | `testing`, `presets`, `manual` |
| 1.0 | The legacy tag is cut and archived (done, B255); the human moves their work (ADR-186 §6). Modules that miss the cutoff, 2026-12-11, ship after 1.0 (ADR-190 A1, ADR-236). Then the stability line. | `freeze`, `release` |

Where horde stands: S0 and most of S1 are done, and S2's labs are being ruled (ADR-266); the lead
put horde at about S1–S2 on 2026-10-09 (B450's response to Bulwark), and no record dates a later
position. This placement is the plan's reading of the rows above, not a record.

## Critical path to 1.0

The longest chain of completion prerequisites among the parts that are **in** for 1.0, computed by
the map from the table above. The map recomputes it on every load, and its self-check 5 proves
that no longer chain exists. As of this refresh, 10 parts, unchanged since 2026-10-17 (12 on
2026-10-09). The map highlights:

`audit` → `seams` → `sub` → `mix` → `rack` → `sluice` → `fxmorph` → `history` → `presets` →
`release`

What it says:
- **Unchanged by this refresh.** No edge on it moved. The one prerequisite added (`arps` needs
  `shell`, B539) makes a chain of five; `fxdevice` is after 1.0 and outside the computation.
- **It is one of twelve tied chains of 10** (re-enumerated 2026-10-27 by a script over the map's
  data): the second link is `seams` or `engine2`; the third is `sub`, `noise` or `filters`; the
  eighth is `history` or `gui3`. So the new engine is on the critical path, tied with the seam
  gate, and the map highlights only the first chain it finds.
- **Its head has moved, though the chain has not.** The new engine is at its bank stage (ADR-284),
  and the sub and the noise oscillator are designed and built in it (ADR-255, ADR-268); they are
  done at cutover. The audit is decided (`ruled`) except spec Q31, and the seam gate (B275 (a),
  (c), (d), B277) is still not dispatched.
- **The long pole is now routing → rack.** The mixer lab (B402) has not started, though the gain
  staging is ruled (ADR-230, ADR-235 item 3) and drawn in the design system (B544, B545, B549), and
  the rack-slot contract is ratified and has grown (RS-4.10 to RS-4.12, RS-5.8; ADR-242, ADR-257,
  ADR-267). The filters, a third tied link, are S1's one open lab.
- **Sluice is on it.** The FX algorithm morph cannot finish without ADR-188's structural class for
  Sluice patches, and Sluice's code enters horde only at Sluice V1, when Sluice goes public (B328,
  Sluice D-096). That date is Sluice's, not horde's.
- **History is near the end of the path,** not because it starts late (A3), but because its
  acceptance lists every feature it must survive.
- **Three of its edges are inferences** (`sub`, `noise` or `filters` → `mix`, and `mix` →
  `rack`), drawn dashed. If the human or the lead rejects one, the path changes.
- Parts that are conditional, open, superseded or after 1.0 (`ott`, `kchorus`, `manual`;
  `engine`, `edges`, `port`; `fxdevice` and the rest of [Post-1.0](#post-10)) are left out of the
  computation.

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
  B327; A6), up to 64 voices, default 16 (ADR-225 item 13); the new Sub as the workshop ruled it,
  a recorded exception to the freeze (B514, ADR-246 item 4, ADR-255); the filters. The noise
  oscillator's sampler is reserved, not shipped (ADR-246, B515).
- **FX:** Shriek; Sluice; Scape; ECHO, now the sibling Echo (ADR-249); Bulwark's compressor; EQ,
  the FX filter and a simplified drive module (A7, A8).
- **Master:** Bulwark's master limiter on the master strip (B438), a true-peak limiter at up to
  1.0 % (ADR-275), always in the output path unless the player turns it off (ADR-292 item 1).
- **Modulation and performance:** the modulators and envelopes, the mod matrix, modulator morph
  (B396), the macros with the intent bus (ADR-192), morph with stepped morph glide (A12) and the FX
  algorithm morph (B394), MPE plus channel aftertouch (A4), the quantizer with Quantize on pitch
  routes (A5, ADR-290) and a simple arpeggiator from the sibling Arp (A5, ADR-264), with the
  per-source stream behaviour aiming for 1.0 as a recorded freeze exception (ADR-270), MTS-ESP
  (B431), the bend laws (the human's summary, B397).
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

Open for the human: whether the manual ships with 1.0 (B451). (The module cutoff is set,
2026-12-11, ADR-236.)

## Post-1.0

| Part | Governing | Note |
|---|---|---|
| Tonality integration (`tonality`) | B390; docs/PARKED.md 13 | After 1.0 by ruling, but the seam room is required NOW: pitch as a seam input, not hard-coded 12-TET; modulation sources Tonality can feed later. |
| Microtuning (`microtuning`) | B390, B431 | horde's own microtuning waits for Tonality. The MTS-ESP client is in 1.0 (`mts`). |
| Granular module (`granular`) | B393, ADR-190 | Based on the granular sibling. |
| Sampler (`sampler`) | B327, B330, B515, ADR-190, ADR-246 | The noise oscillator plays samples, Serum-style: designed now, shipped after 1.0, its keys reserved in 1.0. The plain noise oscillator is in 1.0. |
| Complex arps, step sequencers, generators (`seqgen`) | B391, B539, B559, ADR-190 A5, ADR-264, ADR-293 | "More complex patterns, step seq and generators after 1.0." Owned by Arp. Two arps behind one shared chord stage are after 1.0 too (ADR-293; brief with Arp, seq 9). |
| Vintage-textures FX module (`vintage`) | B444, ADR-190 Am. 2 | Tape warp and VCR artifacts; postponed 2026-10-07, nothing started. |
| Standalone application (`standalone`) | B411, ADR-190 A11 | The first post-1.0 deliverable. |
| horde FX device (`fxdevice`) | B528, ADR-254 | The whole device fed by the DAW's audio, with a MIDI sidechain for notes, soon after 1.0; every decision from 2026-10-20 states its FX-device consequence. |

Also out of 1.0, and not parts of this graph: AAX (A10, B410), Windows on ARM, Linux and AUv3
(B432); the MIDI sidechain that sets the scale (ADR-237 item 8); 'harmonics' and 'ratio' pitch
modes, with a key reserved (ADR-255 item 8); triggered switches and iterators (ADR-267 A2, B542).
True-peak detection on the master, out by ADR-195, is now in: Bulwark built it and ADR-275 allows
its cost.

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

Answered since 2026-10-17 and marked in place, so the numbers do not move (this refresh, against
main `934449a`): 1 (the order of work, ADR-292 item 2), 22 (the noise oscillator's design,
ADR-255), 39 (the AU code, in autonomous's tree), 45 (the WebAssembly toolchain, ADR-280); partly
17 (ADR-266 item 11, ADR-291 item 3); overtaken 21 (the sub ruled by the workshop, ADR-255);
narrowed 5 (ADR-232 ratified) and 47 (ADR-284 item 2). Still deferred by the human: 10 (poly
glide, B504). New since 2026-10-17: 63 to 74, appended at the end.

Narrowed, not removed: [2] the 19 deferred rows are now spec Q31 (item 2); [35] which spec horde 2
answers to is answered for the engine only (item 38); [39] the WebAssembly toolchain is approved in
principle (item 45); [45] the commercial model's licence half is ruled (ADR-219; item 57). Kept:
[5] ADR-189's D1–D3 defaults (item 6): no record says the new spec absorbs them, so the reading
that it does stays a proposal for the human; [6] and [7], the `verify full` runtime and the floor warning (B406): the composed engine's gates
run until cutover, and no record retires them with the port.

1. **Order of work:** DONE 2026-10-27 (ADR-292 item 2): B331's order, S0 to S6, approved as
   proposed; this plan's reordering is withdrawn ([Order of work](#order-of-work)).
2. **Engine audit:** the 19 deferred rows, now spec Q31 (keep, lock or cut each, and envelope
   decision 1's default curves), ruled during the build (SPEC-ENGINE §11.1; B376).
3. **Deferred tests:** B420's deferred tests (basin, onset scatter, per-member envelopes, …).
4. **New engine, the spec's open items:** its questions to rule during the build, each before
   the stage that needs it, and those after it (SPEC-ENGINE §11.1, §11.2; B494). Rounds 14 to 42
   ruled many since 2026-10-17 (ADR-237 to ADR-289); this refresh did not recount what is left.
5. **New engine, the gaps B500 exposed:** where the oscillators return to the host rate; where
   IF-1's per-note volume and pan expressions act; who receives VO-10's lifecycle and IF-1's load
   readouts. NARROWED: ADR-232, RATIFIED 2026-10-17 by the merge of #1117, names the pitch system
   as the lifecycle's consumer; the other two gaps and the load readouts' consumer stay open.
6. **ADR-189 defaults:** DONE 2026-10-17 (ADR-237 item 10): superseded by the new engine's spec; ADR-189 stays as the old engine's history.
7. **verify full runtime:** a budget or a split into staged gates; a `./verify` edit (B406 item 4).
8. **Floor warning:** should the floor's key-mismatch warning fail the gate (B406 item 5).
9. **Seams:** B275 (a) the seam list and "ready" (whether the spec's RC-1 to RC-11 answer it),
   (c) the FOUNDATIONS / horde split, and (d) proceeding on the provisional schema.
10. **Poly glide:** STILL DEFERRED by the human ("happy to save poly glide for another day",
    2026-10-17): do the bank's 22 poly presets that store a glide time glide by default? B504
    (PLANNED) holds it; ADR-232 (RATIFIED) leaves the engine room for poly glide, the spring law and
    quantize. Their reference renders do not glide (B497).
11. **Scale sidechain:** DONE 2026-10-17 (ADR-237 item 8): after 1.0.
12. **MPE:** its design (B388); which expressions reach the swarm and the blades (spec Q2).
13. **Note-on randoms:** the form of the linked pair (B264).
14. **ORBITAL velocity:** BACK TO THE LAB 2026-10-17 (ADR-235 item 4), with the human's idea of a 10×/0.1× mass toggle; not ruled.
15. **Onset scatter:** the recommendation (B370); where it lives is spec Q13.
16. **Modulator morph:** a morph law per modulator type (B396).
17. **Morph editor:** B269's items: per-group cohesion, boundary editing, exempt and locks.
    PARTLY ANSWERED, by the lead's reading: cohesion is one control for the whole field, per-group
    overrides perhaps later (ADR-266 item 11); a per-parameter Exempt is in the morph mode menu
    (ADR-291 item 3, B557). Still open: boundary editing, and locks.
18. **Counter-based draws:** ratify the draw ADR before the first horde 2 patch is saved (B308 H3).
19. **Intent phrases:** the phrase list for the named slots 5–8 (ADR-192).
20. **Intent spec:** approve the SPEC-INTENT-BUS amendment for §3.1 and principle 2 (ADR-192).
21. **The legacy Sub:** what the human dislikes in it (B327, B399). OVERTAKEN 2026-10-20: the
    workshop compared the spec's sub with the human's earlier requests and the human ruled the sub
    (B514, ADR-255) without this question being answered as such; B399 still asks it.
22. **Noise oscillator:** its design (spec X15; its design lab, B452). DONE 2026-10-20 (ADR-255,
    B514): workshopped by ear and ruled; written as spec round 26 (ADR-258).
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
35. **Module names:** PARTLY ANSWERED 2026-10-17 (ADR-235 item 1): one glass each. Still open: the
    names themselves, from B336's three to five per module, which the human picks from the
    FX-screens workshop (ADR-237 item 12).
36. **FX algorithm morph:** p and K by ear (ADR-193 D8), and the items B265 still lists.
37. **Patch model:** ratify it (B263).
38. **Spec:** which spec the rest of horde 2 (the shell, the rack, GUI 3) answers to (B308 M2);
    the engine's is `specs/SPEC-ENGINE.md` (ADR-224 item 8).
39. **AU code:** DONE 2026-10-17 in autonomous's tree: `Mndl` for every new Mindlathe plugin
    (autonomous's response to brief hypersaw-017, seq 2, its Decision 100), as ADR-235 item 5
    proposed. horde's ROADMAP and DECISIONS do not yet note the ruling.
40. **Shell acceptance:** ratify the drafted list with B398 (ADR-209, package P17).
41. **Freeze tag:** DONE 2026-10-17. `v0.9-legacy` is cut and published (ADR-186 §2, B255); kept here so the numbers do not move.
42. **History scope:** DONE 2026-10-17 (ADR-235 item 2): confirmed, and loading a preset into a module is history too; hand edits versus DAW automation to be measured per host.
43. **Serum 2:** load it after seeing stage 1 (B381).
44. **aliasDb:** the re-ruling (B350).
45. **WebAssembly:** the toolchain's exact version. DONE 2026-10-24 (ADR-280 item 1):
    Emscripten 6.0.13, pinned in IF-10.
46. **R2 and R3:** rule after the critic (B357).
47. **Heavy patches:** the policy for the worst known patch, over budget today (B441 phase 3).
    NARROWED: for the new engine, the human ruled CPU bands against B488 at cutover (≤ 15 % passes,
    ≤ 29 % accepted with a follow-up, over 29 % fails; ADR-284 item 2); the current engine's
    policy is not re-ruled.
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
63. **Page chrome lab** (B555–B557, ADR-291): the human rules on the lab
    (`docs/design/page-chrome-lab.html`); its six questions, recorded in its trace
    (`traces/2026-10-27-b555-page-chrome-lab.md`), not in the rows: Exempt collapses the four corners
    to the held value (ADR-109's lean) or holds a value apart; Quantum on a structural parameter is
    disabled with a reason or a no-op; whether a Quantum parameter may carry its own boundaries;
    whether an Exempt parameter still takes modulation; the rail's width and whether it starts
    collapsed off Main and Osc; and the bundle's drags writing no undo node (one gesture, one undo
    node).
64. **Arp's face:** the human rules on the Arp face lab and its round 2 (B553, B558).
65. **Quantize on mixed routes:** whether mixed quantized and unquantized routes into one pitch
    behave otherwise than "any toggle snaps it" (ADR-290, B554).
66. **2-D random source:** its modes, Walk, Jump and Scatter, after listening to the prototype
    (ADR-266 item 8, B540).
67. **Design-system looks:** the components awaiting the human's look (B544, B545, B549), and the
    phase carpet and scale-selector picks (B531).
68. **FX device, per-source routing:** the legacy matrix sends each source to the rack separately;
    RC-5 hands it one mixed pair (B528). Also proposed, not ruled: SPEC-ENGINE RC-5 gains "Must not
    require: the engine to be present" (ADR-254).
69. **Gate-relaxation ledger:** the test author's owed engine-free checks (B-12, B-16, B-27, DT-3's
    80 dB floor, items 25 and 31), each back for a ruling or returned to judged (B520, ADR-288).
70. **Cutover:** the listening batch's judgement, then the human's word (spec §10.6, ADR-284 item
    4).
71. **SPEC-MODULE-MACROS's base line:** a protected path, amended only with the human's sanction
    (B538).
72. **Sluice in the slot:** how "a little harder to find" its XY and library are, when the slot UI
    is designed; the human's lean is not hidden (ADR-236 item 7).
73. **The note context's overflow rule:** a terminal never dropped, note-ons never coalesced, a
    note's offsets coalescing within a block, "to be ruled" (B530 (b), ADR-257 A2).
74. **Two arps, after 1.0:** on Arp's answer to brief seq 9, the human rules the stream tag's form,
    the selector's appended values and the face (B559, ADR-293).


## Inconsistencies found

Flagged for the lead, not resolved here. Each carries its state: **RESOLVED** (fixed in the
records), **TRACKED** (owned by a row, not yet done) or **OPEN** (no row owns it). Items 1–22 are
from 2026-10-01 and 2026-10-09 and items 23–29 from 2026-10-17, all re-checked on 2026-10-27
against main `934449a`; items 30–38 are new in this refresh.

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
7. **RESOLVED (ADR-292 item 2, 2026-10-27).** B331's order is approved as proposed, and B331 says
   so. (This plan's 2026-10-17 reordering is withdrawn; see [Order of work](#order-of-work).)
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
    done (ADR-222; the spec), which B331 does not say. The order is now approved (item 7). B331's
    title still reads "starts once the SCALPEL C++ port has begun", and the port is superseded
    (ADR-223).
23. **RESOLVED (#1117 merged, 2026-10-17).** B504 and ADR-232 are on main; ADR-232 reads
    RATIFIED. B504 is in `bend`'s rows and ADR-232 in `bend`'s and `engine2`'s ADRs, here and in
    the map.
24. **RESOLVED in part (the 2026-10-17 records follow-up).** Every row below now carries a
    "2026-10-17 (plan refresh)" note except B308, whose M2 still asks which protected spec horde 2
    answers to with no note of ADR-224 item 8. The original finding: rows whose open questions the
    rebuild answered or moved carried no note saying so:
    B323 (the voice limit, ruled by ADR-225 item 13); B366 and B377 (envelope decision 1: timing
    ruled by ADR-226 item 10, the curves moved to spec Q31); B376 and B420 (the 19 deferred rows,
    now spec Q31); B380 (note-dependent edge correction, superseded by ADR-222); B404, B406 and
    B385 (the port, superseded by ADR-223; B404's suites move to spec §10.1); B408 (per voice or
    post-sum, ruled by ADR-224 item 3); B225 (the 18 taps, ruled by ADR-230); B372 (the WASM
    toolchain, ADR-225 item 10); B308 M2 (the spec, answered for the engine by ADR-224 item 8);
    B402 (still asks whether the make-up is a user control, which ADR-230 A1 answered).
25. **RESOLVED (B497 reads DONE 2026-10-17).** B498's first marker still reads "PLANNED
    2026-10-16" though its tests have been delivered stage by stage since (item 32).
26. **RESOLVED (records follow-up, 2026-10-27): B399 and B452 now read DISCHARGED by B514; B399 still owes decision 21.** Was: B399 (the Sub workshop) and B452 (the noise oscillator's design lab) both
    still read "OPEN — planned", though the workshop ran as B514 (DONE 2026-10-21), the human ruled
    both (ADR-255) and stage 6 built them and was accepted (ADR-268). B399's first question, what
    the human dislikes in the legacy Sub, was never answered as such (decision 21). Each row needs
    a note that B514 discharged it, or a statement of what it still owes.
27. **OPEN.** `specs/ACCEPTANCE.md` L0-13's "master peak ≤ 1.0 post-tanh" does not bind the new
    engine (ADR-226 item 8: "to be scoped with the human's sanction, as L0-1 was"). L0-1 carries
    its ADR-224 scope note; L0-13 carries none yet.
28. **OPEN.** `specs/SPEC-ENGINE.md` is the new engine's ratified contract (B494, ADR-224 item 8),
    and ADR-230 item 5 treats an edit as "a change to a ratified spec, sanctioned by this ruling",
    but the charter's protected-paths list does not name it. Whether it is a protected path is the
    human's call.
29. **OPEN.** `docs/manual/figures.json` cites the superseded `engine` (33 entries) and `port`
    (19), and `engine2` none (re-counted 2026-10-27). The ids are kept so `manual_scaffold_check`
    holds; re-pointing those entries at `engine2` belongs to the manual's next revision (B451).
30. **RESOLVED (records follow-up, 2026-10-27): B526 is DONE.** Was: B526 reads "OPEN 2026-10-19: directory prepared; awaiting the human's spin-up",
    though Echo spun up on 2026-10-19 and its spin-up notice landed on 2026-10-23 (commit
    `30b5d8a`, `integrations/echo/notice-spinup.md`). B526's acceptance asks that notice and
    "horde's B425 and plan entries are consistent with it"; this refresh makes the plan's entry
    consistent, so the row may be done.
31. **RESOLVED (records follow-up, 2026-10-27): B398 records `Mndl` as ruled by autonomous's Decision 100.** Was: The AU manufacturer code is ruled in autonomous's tree (`Mndl`, its response to brief
    hypersaw-017, seq 2, 2026-10-17, its Decision 100), but no horde row or ADR records the ruling;
    ADR-235 item 5 still reads "Until autonomous rules, horde 2 plans on `Mndl`".
32. **RESOLVED (records follow-up, 2026-10-27): B498 now reads IN PROGRESS.** Was: B498's first marker reads "PLANNED 2026-10-16", while its own later text records the
    acceptance tests delivered stage by stage (stages 1 to 3 on 2026-10-17, and on through the
    bank stage, B494).
33. **RESOLVED (records follow-up, 2026-10-27): B438 carries ADR-275's note, for itself and ADR-195.** Was: ADR-195 and B438 still say true-peak detection on the master stays after 1.0
    ("detection stays sample-peak, and true peak stays post-1.0"), but Bulwark made the master
    limiter a true-peak limiter and ADR-275 (2026-10-23) allows its cost. Neither carries a note
    of ADR-275.
34. **RESOLVED (ADR-292 item 1).** ADR-275 says "the master limiter is always on" while B438's
    master strip has a limiter on/off switch; ADR-292 rules that it is always in the path, off
    only by the player's explicit choice, with the clip latch reporting overs.
35. **RESOLVED (records follow-up, 2026-10-27): B555 lists the six questions.** Was: B555, B556 and B557 each ask the human to rule on a lab, but none records the page
    chrome lab's six open questions; they live only in the lab and its trace
    (`traces/2026-10-27-b555-page-chrome-lab.md`). This plan lists them as decision 63.
36. **RESOLVED (records follow-up, 2026-10-27): the lead's call, recorded on B558: no row while only labs load the bundle.** Was: The `window.Horde` bundle, the design system's components vendored into
    `docs/design/arp-face/` and loaded by the Arp face lab and the page chrome lab, has no row of
    its own; its versions are recorded only in `docs/design/arp-face/README.md`. Whether it needs
    one (re-vendoring, which labs may load it) is the lead's call.
37. **OPEN.** DECISIONS.md places ADR-289 after ADR-294, because ADR-289 was held for spec round
    42 while ADR-290 to ADR-294 were ruled (ADR-290's context says so). The file is append-only,
    so this is order of writing, not an error; a reader scanning by number will find it last.
38. **RESOLVED (records follow-up, 2026-10-27): B507 is marked BUILT.** Was: B507 reads "APPROVED 2026-10-17", but it is built: `6abd916` ("B507: store-touching
    oracles run under a scratch HOME", 2026-10-17), and `./verify` brackets `fast` and `full` with
    `tools/store_logs_check.py`. The row carries no BUILT or DONE marker.

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
