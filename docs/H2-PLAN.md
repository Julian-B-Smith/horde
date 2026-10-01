# horde 2 — plan of record

**Last verified: 2026-10-01**, against ROADMAP.md at records PR #888 (branch `lead-records-158`,
`ecbb205`), which sits on main `c64cfdb`. ROADMAP B387. Re-verified the same day after the lead's
records commit ecbb205 added B397–B403 for the parts that had no row (see
[Inconsistencies found](#inconsistencies-found)).

This is the README-level orientation for horde 2: every part, the rows and ADRs that govern it,
its status, what it needs first, the order of work, and what 1.0 is. It is a MAP of the records,
not a second copy of them.

- **ROADMAP.md outranks this file.** Task state, acceptance criteria and rulings live in the rows
  cited here. Where this plan and ROADMAP disagree, ROADMAP wins, and the disagreement belongs in
  [Inconsistencies found](#inconsistencies-found) until the lead fixes one or the other.
- **The visual map** is [`docs/design/h2-plan-map.html`](design/h2-plan-map.html). Serve the repo
  with `python3 tools/serve_labs.py` and open it. Click a part to see its rows (read live from
  ROADMAP.md), its decisions, and what it unblocks. `?check=1` runs its self-check, which also
  compares the map with the parts table below.
- **Merge order.** This plan cites rows B385–B402, which land with records PR #888. Until #888
  merges, the map's self-check 1 is red for the parts that cite only those rows (MPE, modulator
  morph, OTT, Kuramoto chorus, Tonality, microtuning, granular, arps). That is the check working,
  not a defect.

## How to read a status

| Status | Means |
|---|---|
| built | Exists and passes its gates; nothing more is owed for 1.0 except integration elsewhere. |
| in progress | Work is dispatched, or a built artefact (usually a lab) is waiting on the human's decisions. |
| ruled | The human has ruled what it is; no build is dispatched. |
| planned | Roadmapped, not yet ruled in detail and not started. |
| post-1.0 | After the stability line, by the human's ruling. |
| parked | Archived or tabled by the human. |

No part of horde 2 is **built** yet in this sense. The shell does not exist, so nothing is
integrated. Everything that exists is a lab, a lifted core or a tool.

## The human's summary (2026-09-30), and where each item lives

| The human said | Part | Governing |
|---|---|---|
| "Scalpel and Horde Legacy engines combined into the new primary engine for Horde." | `engine`, `port` | B327, B385 |
| "Bend laws system ported over from H1 (particularly the inertial mass-spring; my own proprietary concept)." | `bend` | B397, ADR-096, B278, B57 |
| "… pitch quantization interface from H1 will be ported over and made more generally available to other FX and devices …" | `quant`, `arps` | B391, B88 |
| "All relevant parameters will be made available to modulation." | `seams`, `modmatrix` | B275 (the manifest declares every parameter's mod limits) |
| "Modulators labs + envelopes labs will deliver the mod page interface." | `modulators` | B208, B226, B370, B377 |
| "All relevant parameters will be made available to morph." | `seams`, `morph` | B275 (the manifest declares every parameter's morph class) |
| "Morph page labs will deliver the morph." | `morph` | B211, B269 |
| "Eventual inclusion of Tonality …" | `tonality`, `microtuning` | B390 |
| "History tree page concept will be ported over from H1 and rigorously tested for edge cases …" | `history` | B389 |
| "All FX modules will be designed and built. Their preset systems will be roped into the larger preset system." | `rack`, `maw`, `sluice`, `reverb`, `fxsimple`, `ott`, `kchorus`, `presets` | B393, B395 |
| "The morph behavior of FX signal chains will be determined from labs." | `fxmorph` | B394, B265, B266 |
| "Global presets will be designed to encompass all capabilities of the synth …" | `presets` | B395, B257, B381 |

The lead's additions, agreed by the human: the new Sub (`sub`), filters (`filters`), the
mixer/routing lab (`mix`), GUI 3 and the screens (`gui3`), the horde 2 shell (`shell`), the legacy
freeze tag (`freeze`), performance (`perf`), the Sluice dependency (`sluice`), the testing
apparatus (`testing`), and the B331 lab-sync order ([Order of work](#order-of-work)).

## The parts

"Needs first" lists completion prerequisites: a part cannot be done before them. In the map, an
edge that cites a row or ADR is solid, and an edge that is this plan's own inference is dashed.
The basis of every edge is in the map (hover an edge, or click a part).

<!-- parts-table:start -->
| Part | id | Status | Governing rows | Needs first |
|---|---|---|---|---|
| Composed engine | `engine` | in progress | B327, B298, B252, B335, B310, B382, B355 | — |
| Engine audit + legacy roundup | `audit` | in progress | B376, B386, B312 | `engine` |
| Edge correction | `edges` | in progress | B383, B380, B378 | — |
| Clean C++ port (phase 1b) | `port` | in progress | B385, B379, B332, B378 | `engine`, `edges` |
| Seam contracts + readiness gate | `seams` | ruled | B275, B276, B277, B339 | `audit` |
| Bend laws + glide | `bend` | planned | B397, B278, B57, B32 | `port`, `seams` |
| Shared pitch quantizer | `quant` | planned | B391, B88, B260 | `bend` |
| MPE | `mpe` | planned | B388 | `bend`, `modmatrix`, `port` |
| Modulators + envelopes | `modulators` | in progress | B208, B226, B366, B370, B377, B253, B259, B264, B267, B126 | `engine` |
| Mod matrix | `modmatrix` | in progress | B392, B207, B57, B270, B282 | `modulators`, `seams`, `audit` |
| Modulator morph | `modmorph` | planned | B396 | `modmatrix`, `morph` |
| Morph: editor, laws, quantum | `morph` | in progress | B211, B235, B268, B269, B272, B240, B308 | `seams` |
| Macros + intent bus | `intent` | planned | B170, B270, B354 | `morph`, `modmatrix` |
| The new Sub | `sub` | planned | B399, B327, B278, B282 | `seams`, `bend` |
| Filters | `filters` | in progress | B274, B287, B288, B289, B290, B303 | `seams`, `bend` |
| Mixer + routing | `mix` | in progress | B402, B225, B23, B258, B263 | `sub`, `filters` |
| FX rack + slot contract | `rack` | planned | B50, B281, B262, B393 | `mix`, `seams` |
| MAW | `maw` | in progress | B318, B373, B393 | `rack` |
| Sluice | `sluice` | in progress | B328, B321, B329, B337, B347, B353, B361, B369, B273, B359, B393 | `rack` |
| Reverb | `reverb` | in progress | B152, B210, B393 | `rack` |
| Simpler FX modules | `fxsimple` | in progress | B210, B336, B393 | `rack` |
| OTT | `ott` | planned | B400, B393 | `rack` |
| Kuramoto chorus | `kchorus` | planned | B401, B393 | `rack` |
| FX algorithm morph | `fxmorph` | in progress | B394, B265, B266, B352 | `morph`, `rack`, `sluice` |
| Patch model → state schema | `patch` | in progress | B263, B308, B258 | `shell`, `morph`, `modmatrix`, `mix` |
| horde 2 plugin shell | `shell` | ruled | B398, B305, B308, B306 | `seams`, `port` |
| Legacy freeze tag | `freeze` | ruled | B255, B308, B305 | — |
| History tree | `history` | planned | B389, B84, B119, B122, B186, B222, B193 | `shell`, `patch`, `fxmorph`, `intent` |
| Testing apparatus | `testing` | in progress | B316, B324, B340, B344, B350, B381, B186 | `port` |
| Performance | `perf` | in progress | B323, B372, B375, B357, B262 | `port`, `edges` |
| GUI 3 + the screens | `gui3` | planned | B302, B271, B322, B336, B261, B308 | `shell`, `fxmorph`, `intent`, `modmorph`, `mix`, `maw`, `sluice` |
| Presets + factory library | `presets` | planned | B395, B257, B261, B312, B349 | `history`, `testing`, `intent`, `maw`, `sluice`, `reverb`, `fxsimple`, `gui3` |
| 1.0 — the stability line | `release` | planned | B305, B327 | `presets`, `gui3`, `perf`, `freeze`, `quant`, `sub` |
| Tonality integration | `tonality` | post-1.0 | B390 | `release`, `quant` |
| Microtuning | `microtuning` | post-1.0 | B390 | `tonality` |
| Granular module | `granular` | post-1.0 | B393 | `release`, `rack` |
| Noise osc / sampler | `noise` | post-1.0 | B327, B330 | `release` |
| Arps, sequencers, generative MIDI | `arps` | planned | B391 | `quant` |
| Archived engines | `archived` | parked | B330, B363, B292 | — |
<!-- parts-table:end -->

## The parts in detail

Each part: what it is · governing rows and ADRs · status · needs first · the human's open
decisions (collected again in [one list](#open-human-decisions)).

### Engine

**Composed engine** (`engine`). Two SCALPEL oscillators, each the SCALPEL blades on horde's
Kuramoto swarm: the "Scalpel and Horde Legacy engines combined" of the summary.
`docs/design/scalpel-horde-engine.js` is the golden the C++ is ported against (ADR-187 §3).
- Governing: B327 (the 1.0 lineup), B298 (the lab voices horde's swarm), B252 (SCALPEL is an
  overhaul of the swarm), B335 (gravity and ensemble timing), B310 (the voice law), B382 (M1–M3
  mirrored from the C++ swarm core), B355 (ADR-189 D1–D3). ADR-184, ADR-187, ADR-189.
- Status: **in progress**. The lineup is ruled and the engine is built in the lab. Its parameter
  surface waits on the audit.
- Needs first: nothing.
- Open: envelope decision 1 (B366, B377); ADR-189's instrument defaults (ruled when the shell is
  built); whether the lab offers law 3 with a real bpm (B382).

**Engine audit and legacy roundup** (`audit`). Every control across the swarm core, the legacy
shell, the composed engine and SCALPEL, decided keep, lock, cull or merge. Then every Legacy
feature not yet in horde 2, approved or denied. The export becomes the provisional manifest
(ADR-186 §5(a)).
- Governing: B376 (built, PR #872), B386 (dispatched), B312 (the user-preset port, built). ADR-186.
- Status: **in progress**, awaiting the human's decisions.
- Needs first: `engine`.
- Open: the B376 decisions (185 concepts); the B386 approvals; the member-count range n.

**Edge correction** (`edges`). How edges are band-limited (B378 F3): the 2-point polyBLEP,
oversampling, linear- or minimum-phase BLEP tables, or note-dependent schemes.
- Governing: B383 (listening lab, merged #878), B380, B378. ADR-187.
- Status: **in progress**. The lab is built and the choice is by ear.
- Needs first: nothing.
- Open: which option, (a)–(f).

**Clean C++ port, phase 1b** (`port`). One C++ composed engine in `h2/engine/`, ported fresh
against the composed JS at current main, reusing proven code from `h2/cores/scalpel/razor_core.h`
and `h2/cores/swarm/` by copy and cleanup. Parity first (RMS and max-abs < 1e-6, identical
events, the bit-exact floor), then the quality suites, then the B378 fixes as ledgered
divergences. A critic reviews it.
- Governing: B385 (dispatched 2026-10-01), B379 (step 1 merged as #876; re-scoped), B332, B378.
  ADR-186 §4, ADR-187.
- Status: **in progress**. razor_core.h and the lifted swarm core become test references.
- Needs first: `engine` (the target); `edges` (F3 enters as a divergence once ruled; B379).
- Open: the bit-exact floor value (B332); the per-core demotion ruling for the composed engine's
  JS (ADR-187 §4), once its Layer-0 suite lands.

**Seam contracts and the readiness gate** (`seams`). An expectation contract per seam, a manifest
per engine (morph class, mod limits including the maximum viable modulation rate, relation to
pitch, velocity, tempo and the voice model, preset participation), and a gate that refuses an
incomplete manifest. This is the mechanism behind "all relevant parameters will be made available
to modulation" and "to morph".
- Governing: B275, B276 (the seam audit, done), B277 (the gate, not dispatched), B339 (FOUNDATIONS
  registered the generic schema). ADR-186 §5(a), §5(d).
- Status: **ruled**. B275 (b) is ruled: every source follows every global unless its manifest
  declares an opt-out with a reason, so the gate is retroactive.
- Needs first: `audit` (its export is the first provisional manifest).
- Open: B275 (a) the seam list and what "ready" means; (c) the FOUNDATIONS / horde split; (d)
  proceeding on the provisional schema before FOUNDATIONS answers.

### Pitch

**Bend laws and glide** (`bend`). H1's pitch system: GlideCore's five laws on both the bend lane
and the note lane, including the inertial mass-spring (the human's own concept), per-note bend
under the same law, and Glide From's three sources.
- Governing: B397 (the bend-laws port: the lanes' laws, glide units, one published per-voice
  pitch, the B57 source), B278 (one published per-voice pitch), B57 (the spring displacement as a mod source,
  revived 2026-10-01), B32 (spring and quantise ordering). ADR-096, ADR-097, ADR-102, ADR-103,
  ADR-180.
- Status: **planned**. It is built in legacy. In horde 2, `glide_core.h` rides in the swarm lift
  and the clean port (B385); B397 covers the rest.
- Needs first: `port` (B397: glide_core.h rides in the clean port); `seams`.
- Open: none recorded.

**The shared pitch quantizer** (`quant`). Legacy's scale quantize (a root plus a 12-bit mask)
becomes a service that other FX and devices can call. Room is left for Tonality: pitch is a seam
input, not hard-coded 12-TET (B390).
- Governing: B391, B88, B260. ADR-106, ADR-158, ADR-093.
- Status: **planned**.
- Needs first: `bend` (the quantiser is anchored on the bend lane, ADR-106).
- Open: whether a MIDI sidechain sets the scale in 1.0 (B260).

**MPE** (`mpe`). Per-note pitch, pressure, timbre and slide, reaching the swarm and blades, the
mod matrix, morph and the voice law.
- Governing: B388. ADR-161, ADR-162, ADR-097, ADR-149.
- Status: **planned**. Its 1.0 membership is open.
- Needs first: `bend`, `modmatrix`, `port`.
- Open: is MPE in 1.0? The human said "roadmap", not "1.0". Also the design itself.

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
- Governing: B392 (a near-term priority, 2026-10-01), B207 (the lab, built), B57, B270, B282.
  ADR-168, ADR-141, ADR-136.
- Status: **in progress**. The lab is built and is next in the human's queue.
- Needs first: `modulators`; `seams` (maximum viable modulation rate, B282); `audit` (inferred:
  the destinations are the audited parameter surface).
- Open: see Modulator morph.

**Modulator morph** (`modmorph`). Modulator shapes and parameters morph continuously between
corners (Hz continuous, sync stepped); mappings jump by quantum flip; a cycle detector rejects
any intermediate state that forms a modulation cycle, proven by construction.
- Governing: B396 (part of B392). ADR-104.
- Status: **planned** (to workshop).
- Needs first: `modmatrix`, `morph`.
- Open: a continuous morph law per modulator type, ORBITAL's bodies included.

### Morph and state

**Morph: editor, laws, quantum** (`morph`). The morph field's blend, quantum and stepped classes;
waypoints and custom curves; the waypoint law as a setting; a position-free draw for horde 2.
- Governing: B211, B235, B268, B269, B272 (TIN·Δ / TPS·Δ ruled), B240, B308 (H3). ADR-104, ADR-109,
  ADR-185, ADR-186 §5(g).
- Status: **in progress**. Editor lab rounds are built.
- Needs first: `seams` (each parameter's morph class).
- Open: B269's items (per-group cohesion, boundary editing in BLEND/STEPPED, exempt and locks);
  the counter-based draw ADR, owed before the first horde 2 patch is saved (B308 H3).

**Macros and the intent bus** (`intent`). Macros, the intent-bus resolver and its unbuilt routing
half (per-corner mod ranges). Macro ranges are per snapshot corner; mappings are per patch.
- Governing: B170, B270, B354. ADR-152, ADR-176, ADR-169, ADR-188 A1.
- Status: **planned** for horde 2. It is built in legacy through phase 2.
- Needs first: `morph`, `modmatrix`.
- Open: none recorded.

**FX algorithm morph** (`fxmorph`). How an FX chain's order and topology morph between corners
while satisfying every condition. The human calls it the hardest part of FX morph.
- Governing: B394 (a priority again soon), B265, B266, B352. ADR-125, ADR-188, ADR-175.
- Status: **in progress**. The labs are built.
- Needs first: `morph`; `rack` (B265); `sluice` (ADR-188: a module patch per horde corner is a
  structural morph class).
- Open: B265/B266's items: the I3 reading, STRICT or TAIL, the approved order set, crossfade
  density.

**Patch model, becoming horde 2's state schema** (`patch`). One system for oscillator parts, FX
racks, mod banks, corners and globals. It lands as a schema version (ADR-186 §5(c)).
- Governing: B263 (the lab is built, commit `8136434`), B308, B258. ADR-186.
- Status: **in progress**, ratification pending.
- Needs first: `shell` (the schema version); `morph`, `modmatrix`, `mix` (what it holds).
- Open: ratify the patch model.

**History tree** (`history`). H1's history: a shell-owned snapshot tree, gesture-level nodes,
fork on restore, re-earned on horde 2 (`gui_history_check`, `undo_check`, B186's gauntlet). It
must survive preset changes, morphing, modulation, mapping changes, signal-flow changes and FX
module edits, with B222's defects as regression rows.
- Governing: B389 (high priority), B84, B119, B122, B186, B222, B193. ADR-160.
- Status: **planned**.
- Needs first, to be DONE: `shell`, `patch`, `fxmorph`, `intent`. The PORT can start as soon as
  horde 2 has state ([Order of work](#order-of-work) A3); its acceptance grows as each feature
  lands.
- Open: confirm that modulated values in motion are not history and mapping changes are (B389).

### Sources and routing

**The new Sub** (`sub`). A new sub oscillator, workshopped. The legacy Sub is not lifted as it is.
- Governing: B399 (the workshop: a design lab starting from SUB OSC), B327, B278, B282. ADR-178.
- Status: **planned**.
- Needs first: `seams` (the retroactive gate, B275 (b)); `bend` (one published per-voice pitch,
  B278).
- Open: what the human dislikes in the legacy Sub (B327, B399 ask this first).

**Filters** (`filters`). Many types with a fidelity programme, serial or parallel placement with
the `+` inlet, Comb as a keytracked filter type, per-note filters for keytracking.
- Governing: B274 (round 2, merged #769), B287 (fixes built), B288, B289, B290, B303. ADR-153.
- Status: **in progress**. Self-oscillation is tabled (B292).
- Needs first: `seams`; `bend` (keytracking reads the published pitch, B278).
- Open: re-ruling the tolerances physics refuses (B287 → B290); the per-note type list after a
  Release cost measurement (B303).

**Mixer and routing** (`mix`). Sources to filters to FX (the dense crosspoint matrix), and the
mixer page with meters and latching clip warnings at every stage.
- Governing: B402 (the horde 2 mixer and routing lab, planned), B225 (the legacy-era mixer lab,
  built), B23 (routing, shipped in legacy), B258, B263. ADR-088, ADR-175.
- Status: **in progress** (B225's lab exists; B402 carries it into horde 2).
- Needs first: `sub`, `filters` (inferred: the strips are the sources, the routing places the
  filters).
- Open: which of the 18 taps become full meters (B225); corner FX buses feeding a global bus
  (B258, discussion owed).

### FX

The roster is ruled (B393): MAW; Sluice (which covers dispersers, shifters, phasers and chorus);
the reverb from the reverb lab; the simpler modules already in the FX lab; NEW, an OTT lab and a
separate Kuramoto chorus. Post-1.0: a granular module based on the granular sibling.

**FX rack and the slot contract** (`rack`). What every module plugs into: one instance per module
type (ADR-172), a slot contract for every type (B281), cyclic topologies per sample (ADR-175), the
cost bench (B262).
- Governing: B50, B281, B262, B393. ADR-172, ADR-175, ADR-128.
- Status: **planned**. horde 2's rack is unbuilt.
- Needs first: `mix` (inferred: the rack sits in the routing); `seams` (B281).

**MAW** (`maw`). The three-stage saturator, FX-C. B318 (design lab, built), B373 (workstation
skin), B393. ADR-170, ADR-092. **In progress.** Needs `rack`. Open: FX-C as a fixed post-stage or a
matrix slot, and B318's other items.

**Sluice** (`sluice`). The morphable FX network, consumed as a PRIVATE, OPTIONAL dependency: the
public build compiles without it (B328). One patch per module XY; different patches at horde's
morph corners (ADR-188). B328, B321, B329, B337, B347, B353, B361, B369, B273, B359, B393.
ADR-188, ADR-166. **In progress** (labs). Needs `rack`.
- The dependency: B328's proposal is filed as Sluice PR #78 (seq 8), ball Sluice. Snapshot and
  macro questions went as seq 10 (B339, respond by 2026-10-12). The Sluice-hold scan (B359)
  keeps Sluice's private text out of this public tree.
- Open: the consumption mechanism, after both leads; binary distribution of a Sluice-built horde;
  the lab's tails (B369).

**Reverb** (`reverb`). B152 (golden audit layers 1–2 done; the port waits on the rack), B210,
B393. ADR-177. **In progress.** Needs `rack`.

**Simpler FX modules** (`fxsimple`). B210, B336 (names and screen styles), B393. ADR-172, ADR-142.
**In progress.** Needs `rack`. Open: the names; ten glasses or six (B336).

**OTT** (`ott`) and **Kuramoto chorus** (`kchorus`). B400 (the OTT lab) and B401 (the Kuramoto
chorus module, in its own lab), both from B393. **Planned**; both labs are to start. Each needs `rack`. Open: are they in 1.0?

### Shell, quality and presentation

**horde 2 plugin shell** (`shell`). A separate product with all of its identity new: the CLAP id
`com.mind-lathe.horde`, a new AU subtype, its own bundle name, pkg identifier and preset-store
root, the vendor "Mindlathe", the display name "Horde". Parameters are generated from manifests
into a committed lockfile; the state carries a schema version from day one; no retire-in-place
before the stability line.
- Governing: B398 (the shell build, planned), B305, B308 (the critic's follow-ups, not
  dispatched), B306. ADR-186.
- Status: **ruled** (ADR-186 ratified 2026-09-27). Nothing is built.
- Needs first: `seams` (the manifests); `port` (B398 follows B385's parity).
- Open: which protected spec horde 2 answers to (B308 M2); the AU manufacturer code (`LfTk` unless
  autonomous rules a fleet code, ADR-186 §3).

**Legacy freeze tag** (`freeze`). The human cuts the legacy tag; CI archives the universal signed
bundles; old projects depend on that archive, not on a rebuild (ADR-186 §2).
- Governing: B255 (the last legacy change, merged #808), B308 (H1, H5, M4), B305. ADR-186.
- Status: **ruled**. The tag is owed by the human. No tag exists in the repository today.
- Needs first: nothing in this graph (B255 has landed). See
  [Inconsistencies](#inconsistencies-found) on B308 H5.
- Open: cut the tag; whether B308 H5's real-blob corpus must land before it.

**Testing apparatus** (`testing`). The patch-space gauntlet, the blind listening passes, the
aliasing metric, the Serum 2 reference test (CPU, sound references, an imitation gauntlet), and
the C++ quality suites.
- Governing: B316, B324, B340, B344, B350, B381 (stage 1 in review, PR #886), B186. ADR-187.
- Status: **in progress**.
- Needs first: `port` (the C++ suites).
- Open: loading Serum 2 once the human has seen stage 1 (B381); the aliasDb re-ruling (B350).

**Performance** (`perf`). C++ CPU, which is Layer-E and never a gate (ADR-187 §8); the voice
limit; a WASM build for the labs; R2 and R3; FX cost.
- Governing: B323, B372, B375, B357, B262. ADR-187.
- Status: **in progress** on the lab side. The C++ numbers wait on the port.
- Needs first: `port`, `edges`.
- Open: horde 2's voice limit, a deterministic count or a documented CPU-adaptive mode (B323);
  installing the WASM toolchain when phase 1b lands (B372); R2 and R3 after the critic (B357).

**GUI 3 and the screens** (`gui3`). The labs assembled into a new GUI that binds only to
generated manifest keys (B308 L2); FX modules as software on screens; the settings page and file
manager.
- Governing: B302 (phase-gated, not dispatched), B271, B322, B336, B261, B308. ADR-116, ADR-186.
- Status: **planned**. The screens workshops are built. The settings lab (B261) has not started.
- Needs first: `shell`, `fxmorph`, `intent`, `modmorph`, `mix`, `maw`, `sluice`.
- Open: one screen style for every FX module, or one each (B322).

**Presets and the factory library** (`presets`). Global presets that cover everything the synth
can do. Saving one also ingests each of its corners. Every FX module's presets are roped into the
larger system. The factory library comes from the presets lab, informed by the Serum gauntlet and
by original concepts.
- Governing: B395 (later), B257 (scrap the old bank; build bottom-up after the engine settles),
  B261, B312, B349. ADR-186, ADR-167.
- Status: **planned**. The human: "I don't want to build presets until we have all the
  functionality in place."
- Needs first: `history`, `testing`, `intent`, `maw`, `sluice`, `reverb`, `fxsimple`, `gui3`.
- Open: what "everything that fits under corner rule" means for auto-ingest.

## Order of work

**PROPOSED by this plan. B331 still reads "Awaiting the human's reorder or approval."** It
reconciles B331's stages with the 2026-10-01 priorities: the mod matrix lab soon (B392), history
as a must (B389), the FX algorithm morph a priority again soon (B394), and the clean C++ port,
already dispatched (B385).

Two orders, on purpose. The [parts table](#the-parts)'s edges are COMPLETION dependencies. The
stages below order the human's REVIEW sessions, and a lab can rule its laws before its
prerequisites are built. B266 raced the FX-morph paradigms on abstract chains, for example. So the
FX algorithm morph is reviewed in S3 and finished after the rack and Sluice exist.

**Track A — the C++ spine** (agent-paced; runs alongside the lab sessions):

| Step | Work | Parts |
|---|---|---|
| A1 | The clean port to parity (B385), then the ADR-187 Layer-0 suite, then the B378 fixes as ledgered divergences. The edge correction joins them once the human picks it. | `port`, `edges`, `testing` |
| A2 | The shell skeleton (ADR-186 §5): the new identity; the manifest feeding a generated parameter lockfile (B308 H4), from B376's export through B277 re-scoped to horde 2; the state schema version; the counter-based draw ADR (B308 H3). | `seams`, `shell` |
| A3 | History on the skeleton (B389): ADR-160's tree ported as soon as horde 2 has state. Its gates are re-earned, then its acceptance grows as each feature lands, so no feature is retrofitted. | `history` |
| A4 | The pitch seam: the bend laws on one published per-voice pitch (B278's lesson), the quantizer as a service, and room for Tonality (B390). | `bend`, `quant` |
| A5 | Performance: Release CPU (ADR-187 §8), the voice-limit ruling, and WASM once the human installs the toolchain (B372). | `perf` |

**Track B — lab sync** (human-paced, one lab per review session; B331 reordered):

| Stage | Was in B331 | Work | Parts |
|---|---|---|---|
| S0 | S0 | Triage, and the ARCHIVED navigator markers (B330). | `archived` |
| S1 | S1 (part) | Engine decisions: the B376 and B386 decisions, then the SCALPEL interface (freezing its parameter set), the edge-correction listening (B383) and envelope decision 1. | `audit`, `engine`, `edges`, `modulators` |
| S2 | S2 (part) | **Modulation, promoted:** the modulators and envelopes lab, then the mod matrix lab (B392, with B57's spring source), then the modulator-morph workshop (B396). | `modulators`, `modmatrix`, `modmorph` |
| S3 | S2 (part) + S4 (head) | **Morph, with the FX algorithm morph promoted from S4** (B394; it shares morph's laws): the morph editor, laws and quantum, then the FX algorithm morph, then the patch model (B263, which becomes the state schema), then the intent bus. | `morph`, `fxmorph`, `patch`, `intent` |
| S4 | S1 (part) + S3 | Sources, routing and MPE: the Sub workshop, filters, the mixer and routing (B225, B258), and the MPE design (B388). | `sub`, `filters`, `mix`, `mpe` |
| S5 | S4 | FX modules and screens: the rack contract, MAW, Sluice (after B328), the reverb, the simpler modules, the OTT lab and the Kuramoto chorus; then the screen aesthetic and logos. | `rack`, `maw`, `sluice`, `reverb`, `fxsimple`, `ott`, `kchorus` |
| S6 | S5 | Presentation, then GUI 3. The settings lab (B261) comes first. | `gui3` |
| S7 | S6 | Calibration and content: the Serum gauntlet (B381) and the listening pass, then the random patch P4/P5, then the legacy preset import (B312), then the presets lab and the factory bank (B395, B257). | `testing`, `presets` |
| 1.0 | — | The legacy tag is cut and archived before the human moves their work (ADR-186 §2, §6). Then the stability line. | `freeze`, `release` |

Why the Sub and filters move from S1 to S4. B331 put them in S1 because the port needed the
parameter set. The clean port's parameter input is now the B376 audit, and before the stability
line the manifest may grow (ADR-186 §5(e), (f)). So the mod matrix and morph labs can sync
against the composed engine's parameters first, and the Sub's and filters' rows join when those
parts sync. The cost: the mod matrix and morph labs see the Sub's and filters' destinations
later, which this plan judges acceptable. It is a proposal for the human.

## Critical path to 1.0

The longest chain of completion prerequisites among the 1.0 parts, computed by the map from the
table above. The map recomputes it on every load, and its self-check 5 proves that no longer chain
exists. At the time of writing, 12 parts:

`engine` → `audit` → `seams` → `bend` → `sub` → `mix` → `rack` → `sluice` → `fxmorph` →
`history` → `presets` → `release`

What it says:
- **The long pole is not the C++ port.** It is the chain from the parameter decisions (B376)
  through the seam gate, the pitch seam, the sources, the routing and the rack.
- **Sluice is on it.** The FX algorithm morph cannot finish without ADR-188's structural class
  for Sluice patches, and that waits on B328's consumption mechanism (ball: Sluice).
- **History is near the end of the path,** not because it starts late (it starts at A3), but
  because its acceptance lists every feature it must survive.
- **Two of its edges are inferences** (`sub` → `mix`, `mix` → `rack`). They are drawn dashed. If
  the human or the lead rejects one, the path changes. (`port` → `bend` was the third until B397
  made it a cited dependency.)
- Parts whose 1.0 membership is open (MPE, OTT, Kuramoto chorus, arps) are left out of the
  computation.

## What 1.0 is

1.0 is **horde 2's first human-cut release tag: the stability line** (ADR-186 §5(f)). Before it,
laws, parameters and ranges change freely, and projects saved earlier may change sound. After
it, a sound-law change is selected by the state header's version, and parameter ids and normalized
ranges are locked by the lockfile.

What ships in it, by ruling:
- **Engine:** two SCALPEL oscillators, each the composed engine, ported to C++ (B327, B385).
- **Sources:** the new Sub (B327).
- **Filters** (B327, B274).
- **FX:** MAW, Sluice, the reverb and the simpler modules (B393). OTT and the Kuramoto chorus are
  ruled to START, and their 1.0 membership is not stated.
- **Modulation:** the modulators, envelopes and mod matrix (B392), and modulator morph (B396).
- **Morph:** morph and the FX algorithm morph (B394).
- **Pitch:** the bend laws and the quantizer (the human's summary, B391).
- **History**, which the human calls "tantamount to it being a useful system" (B389).
- **Presets:** the presets and factory library (B395).
- **GUI 3** (B302).
- **The shell:** the horde 2 shell with its own identity, as CLAP, VST3 and AU (ADR-186 §3).

The gates it passes:
- `./verify full` green;
- the readiness gate on every source (B275);
- each core's ADR-187 status: parity-proven, or demoted with goldens and a Layer-0 suite;
- the 55 shell-linked tools re-earned as horde 2's acceptance list (B308 H5).

Open for the human: MPE in 1.0 (B388); OTT and the Kuramoto chorus (B393); arps and sequencers
(B391).

## Post-1.0

| Part | Governing | Note |
|---|---|---|
| Tonality integration (`tonality`) | B390; docs/PARKED.md 13 | Tonality informs modulation (performance hysteresis, scale and chord complexity). After 1.0 by ruling, but the seam room is required NOW: pitch as a seam input, not hard-coded 12-TET; modulation sources Tonality can feed later. |
| Microtuning (`microtuning`) | B390 | Held until Tonality has accommodated it. |
| Granular module (`granular`) | B393 | Based on the granular sibling. |
| Noise oscillator / sampler (`noise`) | B327, B330 | Any further engine would be sample-based (spectral or granular) or a fresh experiment. |
| Arps, sequencers, generative MIDI (`arps`) | B391 | **Placed here as a PROPOSAL.** B391 leaves its 1.0 placement to the human. They consume the shared quantizer. |

## Parked

- **Archived engines** (`archived`): SPECTRA, STATION, CANTO and the swarmalator. They stay built
  and gated in the frozen legacy tree, and none is lifted (B330; docs/PARKED.md 19, 22, 23; ADR-182).
- **Tabled:** reverse FM (B363) and filter self-oscillation as an intentional option (B292).

## Open human decisions

One list, grouped by part. The row is where the answer is recorded.

1. **Order of work:** approve or reorder the lab-sync order (B331; this plan's proposal above).
2. **Engine audit:** keep, lock, cull or merge for 185 control concepts (B376), including the
   member-count range n.
3. **Legacy roundup:** approve, deny or later for each Legacy feature (B386).
4. **Envelope decision 1:** which default curves, and what a time knob means (B366, B377).
5. **Edge correction:** option (a)–(f), by ear (B383, B380).
6. **ADR-189 defaults:** the D1–D3 defaults in the instrument (ruled when the shell is built,
   ADR-189).
7. **Bit-exact floor:** its value, once phase 1b measures the baseline (B332).
8. **JS demotion:** the composed engine's JS demoted as the quality standard, once its Layer-0
   suite lands (ADR-187 §4).
9. **Seams:** B275 (a) the seam list and "ready", (c) the FOUNDATIONS / horde split, and (d)
   proceeding on the provisional schema.
10. **The legacy Sub:** what the human dislikes in it (B327, B399).
11. **Filters:** re-ruling the filter tolerances physics refuses (B287 → B290).
12. **Per-note filters:** the type list, after the Release cost measurement (B303).
13. **Scale sidechain:** whether a MIDI sidechain sets the scale in 1.0 (B260).
14. **MPE:** is it in 1.0, and its design (B388).
15. **Note-on randoms:** the form of the linked pair (B264).
16. **ORBITAL velocity:** signed components or speed (B267).
17. **Onset scatter:** the recommendation (B370).
18. **Modulator morph:** a morph law per modulator type (B396).
19. **Morph editor:** B269's items: per-group cohesion, boundary editing, exempt and locks.
20. **Counter-based draws:** ratify the draw ADR before the first horde 2 patch is saved (B308 H3).
21. **FX algorithm morph:** the I3 reading, STRICT or TAIL, the approved order set (B265, B266).
22. **Patch model:** ratify it (B263).
23. **History scope:** confirm "modulated values in motion are not history; mapping changes are"
    (B389).
24. **Mixer taps:** which of the 18 become full meters (B225).
25. **Corner FX buses:** the discussion is owed (B258).
26. **MAW:** FX-C as a fixed post-stage or a matrix slot, and B318's other items.
27. **Sluice:** the consumption mechanism (after both leads) and binary distribution (B328).
28. **Sluice tails:** label, stand-in gate, or leave (B369).
29. **Module names:** ten glasses or six (B336).
30. **OTT and the Kuramoto chorus:** in 1.0 or later (B393).
31. **Screen styles:** one for every FX module, or one each (B322).
32. **Spec:** which protected spec horde 2 answers to (B308 M2).
33. **AU code:** the manufacturer code (ADR-186 §3; autonomous may rule a fleet code).
34. **Freeze tag:** cut it, and decide whether B308 H5's real-blob corpus must precede it.
35. **Voice limit:** a deterministic count or a documented CPU-adaptive mode (B323).
36. **WASM:** install the toolchain when phase 1b lands (B372).
37. **R2 and R3:** rule after the critic (B357).
38. **Serum 2:** load it after seeing stage 1 (B381).
39. **aliasDb:** the re-ruling (B350).
40. **Corner auto-ingest:** what "everything that fits under corner rule" means (B395).
41. **Arps and sequencers:** 1.0 or post-1.0 (B391).

## Inconsistencies found

Flagged for the lead, not resolved here. The lead's records commit `ecbb205` (PR #888) answered
them: each item below now carries its state, **RESOLVED** (fixed in ROADMAP), **TRACKED** (owned by a
row, not yet done) or **OPEN**.

1. **OPEN until #888 merges. Rows B385–B402 exist only on `lead-records-158`** (PR #888, open).
   This plan, and the map's check 1, cite them. Merge #888 before this PR, or the map's check 1 stays red for MPE,
   modulator morph, OTT, the Kuramoto chorus, Tonality, microtuning, granular and arps.
2. **RESOLVED (ecbb205): B379 now reads "STEP 1 MERGED (#876); SUPERSEDED by B385 (the clean
   port)".** Was: B379's status marker is stale. It still reads "BLOCKED (steps 3–5) — rulings; step 1 in
   progress", but the row itself records step 1 MERGED (#876) and the 2026-10-01 re-scope to B385.
3. **TRACKED under B403 (B385 updates it). `h2/README.md` is stale against B385.** It is "Last verified: 2026-09-30".
   - It still says the composed layer "waits on the razor_core ruling".
   - It gives the phase-1b target as `c79be56`, "not yet re-pinned".
   - B385 makes `razor_core.h` and the lifted swarm core test references for a fresh
     `h2/engine/`.
   - Its rule 4 says "There are no divergences yet", while `docs/port/divergences.json` holds seven
     (D1–D4, M1–M3) for the composed engine, which is the port's target.
4. **TRACKED under B403 (the lead refreshes it in a records PR). The charter's §Domain
   (CLAUDE.md) predates ADR-186, B327 and B330.**
   - It calls HYPERSAW "the frozen plugin id". ADR-186's consequences and B308 M2 say amend it.
   - It describes SCALPEL as a CANDIDATE whose integration shape is "the open question", which
     B298, B327 and B385 settled.
   - It presents SPECTRA, CANTO and STATION as live members of the engine family, though B330
     archived them.
5. **TRACKED under B403. B302 frames GUI 3 as replacing gui2.** It reads "gui2 … stays the shipping GUI until gui3
   passes the same gates … then a switch-over ADR". Under ADR-186, gui2 belongs to the frozen
   legacy shell and horde 2 is a separate product, so there is no switch-over inside one shell.
   B308 L2 has GUI 3 bind only to generated manifest keys.
6. **TRACKED under B403. Legacy rows not yet re-scoped per ADR-186's consequences.** ADR-186 says "Queued legacy work
   becomes horde 2 work, or is dropped: B278, B279, B288's legacy-comb migration, B289 …", and
   §1 keeps render-changing defects unfixed in legacy. Still written against the legacy tree:
   - B277 enumerates `kEngineBlocks`/`kParams`.
   - B278 asks for an `engine_revision` gate.
   - B289 targets `src/svf_core.h`.
   - B288 rules a legacy comb migration.
   - B282 smooths the legacy Sub.
7. **OPEN. The B331 order and the 2026-10-01 priorities disagree.** B331 puts the Sub workshop and
   filters (S1) before modulation (S2) and the FX chain morph in S4. The 2026-10-01 rulings make
   the mod matrix lab (B392) and the FX algorithm morph (B394) near-term priorities. B331 is still
   awaiting the human; this plan's [Order of work](#order-of-work) is a proposal, not a ruling.
8. **OPEN. The freeze has different prerequisites in different records.**
   - ADR-186's ratification note says the tag is owed "after B255 lands", and B255 merged as #808
     on 2026-09-28.
   - B308 says "H5's corpus and H1's tag come before the freeze", and H5 is not dispatched.
   - ADR-186 §1(α) needs that corpus to prove any later legacy fix render-neutral.
9. **TRACKED under B403. B263 carries no BUILT marker,** though its lab is built (`docs/design/patch-model-lab.html`,
   commit `8136434`).
10. **RESOLVED (ecbb205) for six parts; two remain without a row. Parts with no governing row of
    their own.** The new rows, now cited in the table and the map:
    - the bend-laws port: **B397**;
    - the horde 2 shell build: **B398**;
    - the new Sub workshop: **B399**;
    - the OTT lab: **B400**;
    - the Kuramoto chorus: **B401**;
    - the horde 2 mixer and routing lab: **B402**.

    Still without a row of their own, as originally listed: the WASM build (one sentence in B372)
    and horde 2's voice limit (spread across B323, B372 and B375). Both are cited through those rows
    under `perf`.
11. **OPEN. The 1.0 FX list differs between rows.** B327 lists the 1.0 FX as "filters, MAW, Sluice,
    rack". B393's roster adds the reverb, the simpler modules, the OTT lab and the Kuramoto chorus,
    and does not say which of the new ones are 1.0.
12. **RESOLVED (ecbb205): B391 now leaves its 1.0 placement to the human,** naming this plan's
    open-decision list. Was: B391 deferred sequencing "to the plan of record", which cannot rule.
    Arps stay placed post-1.0 as a proposal (decision 41).

## Keeping this true

- The lead updates this file when a ruling changes a part's status, rows or prerequisites, and
  updates the `DATA` block in `docs/design/h2-plan-map.html` in the same change. The map's
  self-check 4 fails if the two disagree.
- The map reads row titles and statuses live from ROADMAP.md. A part's status here is this plan's
  summary of those rows. When they move, the "Last verified" line above is what dates this
  summary.
