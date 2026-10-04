# horde 1.0 — the pre-freeze pass and a feature-freeze proposal (B440)

> **Origin:** horde lead session, 2026-10-04, ROADMAP B440 (records branch
> `lead-records-181`, PR #926). The human asked: *"Should we do one more pass and then
> declare a feature freeze within the next couple days?"* The lead answered yes; this is
> that pass, written by a dispatched agent. ROADMAP.md (as on `lead-records-181`) outranks
> this file. Where they disagree, ROADMAP wins and the disagreement is a finding here.
>
> **Status: PROPOSED, for the human.** Nothing here is a ruling. The freeze is declared
> in DECISIONS only after the human approves it (B440).
>
> The one-page summary is [`docs/design/freeze-proposal.html`](../design/freeze-proposal.html).

## The answer in six lines

1. **Declare the feature freeze on Wednesday 2026-10-07**, as a ROSTER freeze: the list
   of what is in 1.0 is locked; no new module or feature joins 1.0 after it.
2. **First, amend one sentence of the module 1.0 bar (B439).** As written, it says every
   MUST row must hold "by the feature freeze". No module can meet that in three days:
   horde 2 has no shell, no rack and no hosted module yet. Read literally, a 10-07 freeze
   would drop the Sub, which B327 makes part of the 1.0 lineup. Decision A1 below
   proposes three dates: the roster freezes on 10-07; each module's scope list freezes at
   its lab sync; the MUST rows are judged at a later module cutoff.
3. **Inventory:** 17 IN, 1 CONDITIONAL, 5 OUT, 3 with no deciding row (the Kuramoto
   chorus, MPE, arps). Four of the 17 INs are inferred, not stated. Decision A8 asks the
   human to confirm them.
4. **Decisions:** 52 open human decisions block 1.0. Only 12 of them (group A) must be
   answered to declare the freeze. All 12 are the human's own calls, and none waits on a
   sibling or a measurement.
5. **Gaps:** B427–B433 each have an owner row, but none is dispatched, and none is yet
   placed in H2-PLAN's order (the plan was last verified 2026-10-01). Five needs have no
   owner at all: a Windows test machine, min-spec measurement hardware, the efficiency of
   the blade inner loop, CPU measured with two oscillators, and funded counsel.
6. **CPU:** the heavy patches are over the 34 % source slice, and that stays true under
   every caveat in the record. By how much is uncertain: 1.1× to 3.1×, because the
   measurements ran on a loaded machine. The plan is: re-measure clean; then the kernel
   work; then the oversampling default (it rides on the edge-correction ruling); then a
   deterministic polyphony and quality policy.

## What "feature freeze" means here

Three different things are called a freeze in the records. This proposal keeps them apart.

| Name | What it is | Governing |
|---|---|---|
| **Feature freeze** (this proposal) | The 1.0 roster is locked: which modules and features ship. New ideas go to the post-1.0 list. | B440, B439 |
| **Legacy freeze tag** (`freeze` in H2-PLAN) | The human cuts the legacy tag; CI archives the signed bundles. | ADR-186 §2, B308, B255 |
| **The stability line** (1.0) | The first human-cut horde 2 release tag; ids and ranges lock. | ADR-186 §5(f) |

**Proposed rules from the freeze on** (each is for the human to approve):
- No new module, engine, modulator type or headline feature enters 1.0. New ideas get a
  ROADMAP row marked post-1.0. (Sluice already does this, D-108.)
- A CONDITIONAL item ships only if it meets the B439 bar at the module cutoff (the OTT
  rule, B423).
- A module's scope list (bar row 1) freezes when its lab syncs (B331 stage), not on 10-07.
- These are not features, and they continue: fixes, fidelity, CPU work, the B427–B433
  production gaps, tests, presets and GUI 3.
- **Default for anything still undecided on 10-07: post-1.0** (decision A2). This makes
  the date hold even if a roster question slips.

## 1. Module inventory

Basis: **explicit** means a row rules the 1.0 status. **Inferred** means the status
follows from rows, but no row states it. "Scope list" is the B439 bar's row 1: a frozen
list of the controls and modes that ship. YES means the list exists, PARTIAL means
decisions exist but no frozen list, NO means nothing yet.

### Sources

| Module | Owner | Governing rows | Scope list | 1.0 | Basis |
|---|---|---|---|---|---|
| Composed engine: the swarm with SCALPEL blades, two oscillators | horde | B327, B385, B405, B404, B376, B386, B416, B420 | PARTIAL. B376 rounds 1 and 2 are decided (round 2 on 2026-10-02), and B386 is ratified. B420's by-ear tests are still owed. | **IN** | explicit, B327 |
| The new Sub | horde | B327, B399, ADR-178, B275(b) | **NO**. The workshop has not started, and the human has not yet been asked what they dislike in the legacy Sub. | **IN** | explicit, B327 |
| Filters | horde | B327, B274, B287, B290, B303 | PARTIAL. B274's lab has 12 types. The per-note type list (B303) and the tolerances (B290) are open. | **IN** | explicit, B327 |
| Noise oscillator / sampler | — | B327, B330, B409 | — | **OUT** | explicit, B327 (B409 asks whether a *plain* noise osc enters 1.0: decision A6) |

### FX

| Module | Owner | Governing rows | Scope list | 1.0 | Basis |
|---|---|---|---|---|---|
| MAW | sibling **Maw** (hosted as FX-C) | B318, B373, B393, ADR-170 | **NO**. SPEC-MAW is a full spec with no 1.0 scope section. B318's items are open, including FX-C as a fixed post-stage or a matrix slot. | **IN** | explicit, B393 |
| Sluice | sibling **Sluice**. Its code enters horde's tree at Sluice V1 (Sluice D-096). This corrects B328's private-dependency proposal. | B328, B393, B436, B369, B347 | PARTIAL. Sluice D-108 parks the resonator and the noise source until after horde 1.0, but there is no frozen 1.0 list. | **IN** | explicit, B393 |
| Scape (reverb) | sibling **Scape** | B393, B421, B152 | **NO**. Scape has no spec, no fidelity suite and no C++ core yet (its README). | **IN** | explicit, B393 + B421 |
| ECHO (the standard delay) | horde | B425, B386 item 10, ADR-142 | PARTIAL. B425 has a one-sentence scope: tempo sync, ping-pong, mid/side, feedback filtering, modulation. | **IN** | **inferred**. B425 says "a standard effect, as the reverb will be", and B393's FX-lab modules included the delay. No row says "1.0". |
| Bulwark compressor face | sibling **Bulwark** | B234, B393, B421 | **NO**. Bulwark's spec (Phase D) has not started. | **IN** | **inferred**. B393's "simpler modules already in the FX lab" included B234's compressor, and B421 moved it to Bulwark. No row says the compressor is in 1.0. |
| OTT | sibling **Bulwark** (B393 amendment) | B400, B423 Q-001, B439 | **NO**. Bulwark M2 has not started. | **CONDITIONAL** | explicit, B423 Q-001 |
| Simple FX-lab modules: Drive, FX Filter, EQ, Comb (Notch?) | horde (no row moves EQ or Drive out) | B393, B210, B234, B336 | **NO** | **IN as a group** | explicit for the group (B393). **Which modules are in it is undecided.** Drive may become a one-stage MAW preset (B210). Comb may live only as the filters' keytracked comb type. Notch is in no current row. "Ten glasses or six" is open (B336). Decision A7. |
| Kuramoto chorus | horde (own lab, B401) | B393, B401 | **NO**. The lab has not started. | **UNDECIDED** | **no row decides**. B393 says "to start". Decision A3. |
| Granular module | — | B393 | — | **OUT** | explicit, B393 (post-1.0) |
| Sluice resonator and noise source | sibling Sluice | Sluice D-108 | — | **OUT** | explicit (Sluice D-108, the human) |
| Legacy Gain slot | — | B386 item 9 | — | **OUT** | explicit (DROP) |
| Legacy Room | — | B421, B393 | — | **OUT** | explicit. Scape replaces it. B421: Room is measured once as a comparison, and retiring it stays horde's job. |

The legacy Delay is not a separate row: ECHO absorbs it (B425). The legacy Echo
(the time engine) also folds into ECHO.

### Master

| Module | Owner | Governing rows | Scope list | 1.0 | Basis |
|---|---|---|---|---|---|
| Master limiter (Bulwark's limiter face, fixed last) | sibling **Bulwark**; the master strip is horde's (B402) | B438, B423 Q-002, B435 A1, B402 | **YES**. B438 names the controls: on/off, Ceiling (default −0.3 dBFS), Release (auto or ms), a gain-reduction meter, the clip latch, and Volume, the cut-only output gain. | **IN** | explicit, B438 |

### Modulation

| Module | Owner | Governing rows | Scope list | 1.0 | Basis |
|---|---|---|---|---|---|
| Modulators: LFOs, Kuro-LFO, ORBITAL, MIDI trackers, note-on randoms, the dry follower, coherence R | horde | B208, B226, B126, B236, B264, B267 | **NO**. The lab offers the family, but no list says which types ship. The linked note-on randoms (B264), ORBITAL velocity (B267) and the Kuro-LFO's scope (B236) are open. | **IN** | explicit (the human's 2026-09-30 summary; H2-PLAN "What 1.0 is") |
| Envelopes (the ENV hierarchy) | horde | B366, B377, B370 | PARTIAL. 17 conflict decisions are recorded, and decision 1 is open. | **IN** | explicit (as above) |
| Mod matrix | horde | B392, B207, B57 | **NO** | **IN** | explicit, B392 + the human's summary |
| Modulator morph | horde | B396 | **NO** | **IN** | explicit (H2-PLAN "What 1.0 is", from the human's summary) |
| Macros and the intent bus | horde | B170, B270, B354, ADR-169 | **NO** for horde 2 (built in legacy) | **IN** | **inferred**. Presets and GUI 3 need it first (H2-PLAN), and the bar's four-role face assumes it. The human's 1.0 list does not name it. |
| MPE (and channel aftertouch) | horde | B388, B409, B429(5) | **NO** | **UNDECIDED** | **no row decides**. B388 says "roadmap". Decision A4. |
| Arps, sequencers, generative MIDI | horde | B391 | **NO** | **UNDECIDED** | **no row decides**. B391 leaves it to the human, and H2-PLAN proposes post-1.0. Decision A5. |

### Mixer and routing

| Module | Owner | Governing rows | Scope list | 1.0 | Basis |
|---|---|---|---|---|---|
| Mixer and routing, with the master strip | horde | B402, B225, B258, B438, B435 | PARTIAL. B402 plus B438's master strip. Which of B225's 18 taps become meters is open, and B258's corner buses are not yet discussed. | **IN** | explicit (H2-PLAN, agreed by the human; B438) |
| FX rack and the slot contract | horde | B50, B281, B262, B435 | PARTIAL. ADR-172, B281's contract and the I/O gain standard exist. horde 2's rack is unbuilt. | **IN** | explicit (B393 hosts the roster in it) |

### Counts

| | count | which |
|---|---|---|
| **IN** | 17 | engine, Sub, filters, MAW, Sluice, Scape, ECHO*, Bulwark compressor*, the simple-FX group†, master limiter, modulators, envelopes, mod matrix, modulator morph, macros and intent bus*, mixer and routing, FX rack |
| **CONDITIONAL** | 1 | OTT |
| **OUT** | 5 | noise osc/sampler, granular, Sluice resonator and noise, legacy Gain, legacy Room |
| **No deciding row** | 3 | Kuramoto chorus, MPE, arps/sequencers |

\* inferred, not stated (A8 asks the human to confirm). † the group is IN; its members are
undecided (A7).

**Scope lists:** 1 YES (the master limiter), 7 PARTIAL, and every other IN module NO. So
the bar's row 1 is far from met. That is why A1 moves the scope freeze to each module's
lab sync.

### Non-module features, for the roster freeze

| Feature | 1.0 | Row |
|---|---|---|
| Morph, the FX algorithm morph, history, presets, GUI 3, the bend laws and glide, the shared quantizer | IN | the human's 2026-09-30 summary, B389, B394, B395, B302, B397, B391 |
| MTS-ESP client | IN | B431 (ruled 2026-10-04) |
| Platforms: macOS (universal if clean) and Windows x64; CLAP, VST3, AU | IN | B432, ADR-186 §3 |
| Accessibility baseline (Surge XT as the model) | IN | B432 |
| Sustain pedal, init patch, MIDI learn, mono/legato/portamento, per-patch bend range | IN (sustain is ship-blocking) | B431 |
| The 57 PLANNED legacy-roundup items and the 11 ruled ones | IN as ruled | B386 |
| True stereo | **open**: which parts are 1.0 | B408 (decision A9) |
| AAX | **open** | B410 (A10) |
| Standalone app | **open**: planned, 1.0 unstated | B411 (A11) |
| Stepped morph glide | **open**: planned, 1.0 unstated | B424 (A12) |
| Tonality, microtuning beyond MTS-ESP | OUT | B390 |

## 2. Open human decisions that block 1.0

One list. **Group A must be answered to declare the freeze.** Group B blocks 1.0, but
not the freeze. Every B item belongs to an IN module or feature, and the freeze does not
wait on it.

### Group A — the roster (12, by 2026-10-07)

| # | Row | Question |
|---|---|---|
| A1 | B439, B440 | The bar says every MUST holds "by the feature freeze". Should the freeze instead be three dates: the roster locks on 10-07, each module's scope list freezes at its lab sync (B331), and the MUST rows are judged at a module cutoff set later? |
| A2 | B440 | Does anything still undecided on 10-07 go post-1.0 by default? |
| A3 | B393, B401 | The Kuramoto chorus: IN, CONDITIONAL under the OTT rule, or post-1.0? (Its lab has not started. It is not on the critical path.) |
| A4 | B388, B409, B429(5) | Is MPE in 1.0? Either way, is channel aftertouch a 1.0 mod source? (B429(5): the MIDI dialect choice decides the wrapper's proxy-parameter flood.) |
| A5 | B391 | Arps, sequencers and generative MIDI: post-1.0, as H2-PLAN proposes? And confirm the shared quantizer itself is IN. |
| A6 | B409, B327 | Does a plain noise oscillator enter 1.0, separately from the post-1.0 sampler? |
| A7 | B393, B210, B336 | Which simple FX modules ship? Drive (its own module, or a one-stage MAW preset?), the FX Filter, EQ, Comb (a module, or only the filters' comb type?), Notch. Ten glasses or six? |
| A8 | B425, B421, B234, H2-PLAN | Confirm three inferred INs: ECHO, Bulwark's compressor face, and macros with the intent bus. |
| A9 | B408 | True stereo: IN, with the width equation and the per-voice vs post-sum choice set by the B408 lab? Or CONDITIONAL? |
| A10 | B410 | AAX: in or out of 1.0? |
| A11 | B411 | A standalone application: in or out of 1.0? |
| A12 | B424 | Stepped morph glide (the quantum/blend hybrid): in 1.0, or after? |

### Group B — design decisions inside 1.0 (40)

H2-PLAN's "Open human decisions" (41 items, as of 2026-10-01) were re-checked against
today's rows. **Closed since:** 2 (B376 round 2 decided 2026-10-02), 3 (B386 ratified
2026-10-03), 7 (the floor, ADR-187 A2), 27 (Sluice's code enters at V1, Sluice D-096,
correcting B328), and OTT's half of 30 (B423). **Not 1.0-blocking, so left out:** 36
(WASM, B372) and 38 (Serum 2 stage 2, B381). Everything else still open is below, with the
newer rows added.

| # | Row | Question |
|---|---|---|
| B1 | B331 | Approve or reorder the lab-sync order (H2-PLAN's proposal). It sets when each scope list freezes. |
| B2 | B383, B380 | Edge correction, by ear: option (a)–(f). This also sets the oversampling default, a CPU lever. |
| B3 | B323, B375 | horde 2's voice limit: a deterministic count per patch, or a documented CPU-adaptive mode? Must every factory preset fit the 34 % slice at 8 voices? |
| B4 | B357 | R2 and R3: blade caps decoupled from os, and a steep polyphase decimator, after the critic. |
| B5 | B366, B377, B370 | Envelope decision 1 (the default curves, and what a time knob means), and the onset-scatter recommendation. |
| B6 | B420 | The deferred by-ear picks: driftMode, inertia vs freqGlide, the topology reduction, the basin, onsetScatter vs phase scatter, attack and release scatter. |
| B7 | ADR-187 §4 | Demote the composed engine's JS as the quality standard, once its Layer-0 suite lands? |
| B8 | B406 (3) | The float32 cull fade's block-size dependence: correct it in the JS and C++ together, as a ledgered divergence? |
| B9 | B399, B327 | What does the human dislike in the legacy Sub? This gates the Sub's scope list. |
| B10 | B290 (via B287) | Re-rule the filter tolerances that physics refuses. |
| B11 | B303 | The per-note filter type list, after the Release cost measurement. |
| B12 | B260, B431 | Does a MIDI sidechain set the scale in 1.0? And what is the precedence between horde's scale and MTS-ESP? |
| B13 | B275 (a), (c), (d) | The seam list and what "ready" means; the FOUNDATIONS / horde split; proceeding on the provisional schema. |
| B14 | B308 H3 | Ratify the counter-based draw ADR before the first horde 2 patch is saved. |
| B15 | B263 | Ratify the patch model (it becomes the state schema). |
| B16 | B269 | The morph editor: per-group cohesion, boundary editing, exempt and locks. |
| B17 | B265, B266 | The FX algorithm morph: the I3 reading, STRICT or TAIL, and the approved order set. |
| B18 | B396 | A continuous morph law per modulator type. |
| B19 | B264 | The form of the linked note-on random pair. |
| B20 | B267 | ORBITAL velocity: signed components, or speed? |
| B21 | B236 | The Kuro-LFO's scope: global, or per voice? |
| B22 | B389 | Confirm: modulated values in motion are not history; mapping changes are. |
| B23 | B225 | Which of the 18 taps become full meters? |
| B24 | B258 | Corner FX buses feeding a global bus. The discussion is owed. |
| B25 | B318 | MAW: FX-C as a fixed post-stage or a matrix slot, and B318's other items. |
| B26 | B369 | Sluice tails: a label, a stand-in gate, or leave them? |
| B27 | B436 (1) | The listening verdict on the macro-range inverted pair. |
| B28 | B439 appendix; Sluice Q-024 | Sluice against the per-module budget. Its own bench reads about 3 % of a core at 1× for the v1 roster, and every factory preset is within 3 %. If that core is this Mac's M3, ×1.5 gives about 4.5 % of min-spec, over both the 2 % default line and the 4 % worst line. Does Sluice get an exception, or must it cut? *(The machine behind Sluice's figure is not restated in its row. To verify.)* |
| B29 | B439 appendix | The ×1.5 M3-to-min-spec factor is an assumption. Who measures it once on an M1 base or the Intel ultrabook, and on what hardware? |
| B30 | B336, B433 | The module names. Approve the lead's zero-cost knockout search per name (no counsel budget). |
| B31 | B322 | One screen style for every FX module, or one each? |
| B32 | B308 M2 | Which protected spec does horde 2 answer to? |
| B33 | ADR-186 §3 | The AU manufacturer code (`LfTk`, unless autonomous rules a fleet code). |
| B34 | ADR-186 §2, B308 | Cut the legacy freeze tag. Must B308 H5's real-blob corpus land before it? |
| B35 | B350 | The aliasDb re-ruling. |
| B36 | B395 | Corner auto-ingest: what "everything that fits under corner rule" means. And the factory library's size: commission sound designers or not? |
| B37 | B412, B433, B101 | The commercial model (sold, donation, open source), the licence, copy protection, signed installers, and the update path. |
| B38 | B406 (4), (5) | `./verify full` now runs over 600 s: set a budget, or split it into backgrounded stages? And should the floor's key-mismatch warning fail the gate? |
| B39 | B418 | The sluice_hold gate: keep it failing (and exclude visitors' filings), demote it to a warning, or retire it? |
| B40 | ADR-189, B355 | The D1–D3 instrument defaults, ruled when the shell is built. |

**Total: 52 decisions (12 A + 40 B).**

## 3. 1.0-critical gaps: owners and order

B426 filed B427–B433 as the ship-blocking gaps no row owned. Each now has a row. **None is
dispatched, and none appears in H2-PLAN's parts or order of work**, because the plan was
last verified on 2026-10-01, before them (owner of that refresh: B387, the lead).

**The proposed order** puts first whatever gets more expensive the later it is done:

| Order | Gap | Owner row | Plan step | When | Owner flag |
|---|---|---|---|---|---|
| 1 | Trademark knockout search per user-visible name: horde, Mindlathe, SCALPEL, MAW, ECHO, Sluice, Scape, Bulwark, and the simple modules' names | B433 | release | **now**, before the names freeze (B336, A7) | human (self-run, per the lead's proposal, not yet approved). **Counsel: no owner, unfunded.** |
| 2 | Bump clap-wrapper to v0.16.0 or later (6); give every modulation destination an automatable-parameter fallback, since the wrappers drop `PARAM_MOD` (4) | B429 | before A2 (the shell skeleton) and before the manifest freezes | before B398 | row only |
| 3 | Frozen identities: the lockfile diff against the last tag with every id below 0xB00000; AU parameter order; an explicit VST3 class ID | B428 | A2, with B398 | at the shell skeleton | row only |
| 4 | Runtime safety in the skeleton: the non-finite output guard (1), FTZ/DAZ save and restore per `process()` (3), the probe catching nothrow `new` (7) | B430 | A2 | at the shell skeleton | row only |
| 5 | clap-validator in CI on macOS and Windows, with a planted-fault control | B427 | A2, `testing` | from the first shell artefact | Windows CI exists; **real-host loads on Windows: no owner** (see below) |
| 6 | Musician basics: the sustain pedal (ship-blocking), an init patch, mono/legato/portamento, per-patch bend range, MIDI learn, the MTS-ESP client | B431 | A4 (pitch seam, B397) | with the bend-laws port | row only |
| 7 | Tails, SLEEP, latency, event-time independence | B429 (1)–(3) | rack | when the rack hosts its first tailed module (Scape, Sluice or ECHO) | row only |
| 8 | Concurrency stress, click-free steals, state fuzzing, cross-platform goldens | B430 (2), (4)–(6) | A3, `testing` | once horde 2 has a state schema | (6) needs non-darwin runners |
| 9 | GUI on real platforms: WebView2, many instances, focus, mixed DPI, feed gating, unique names, accessibility | B432 | S6, `gui3` | with GUI 3 (B302) | **Windows mixed-DPI and WebView2 testing: no owner machine** |
| 10 | Distribution: a notarised pkg, Windows signing, a third-party notices file, the licence | B433, B412, B101 | `release` | release candidate | signing secrets and the licence are the human's (B37) |
| 11 | pluginval at strictness 10 on VST3 and AU, both OSes; real host loads; auval on the signed AU | B427 | `release` | release candidate | **Windows hosts: no owner** |

**Other ship-blocking items found in this pass:**

| Item | Owner row | State | Flag |
|---|---|---|---|
| **The master limiter's core.** Bulwark's C++ core (M1) and limiter face (M3) have not started, and Bulwark orders the limiter AFTER OTT (M2). | B438, B423 | The limiter is IN; OTT is only CONDITIONAL. | The order is upside down for horde. **The lead should ask Bulwark (a notice, B423) to build the limiter before OTT.** |
| History, "tantamount to it being a useful system" | B389 | planned | owned; starts at A3 |
| The legacy freeze tag, before the human moves their work | ADR-186 §2, B308 | owed by the human | B34 |
| B308 H5: the real-blob state corpus | B308 | not dispatched | owned; no dispatch |
| The readiness gate, re-scoped to horde 2 | B277, B403 | not re-scoped | owned by the lead (B403) |
| `./verify full` over the 600 s agent ceiling, so agents cannot run the done gate in one call | B406 (4) | open | B38 |
| Stale records (the VST3 SDK licence text) | B434 | open | lead |
| H2-PLAN refresh for B404–B440 | B387 | the plan is "Last verified 2026-10-01" | lead |

**Needs with no owner at all:**
1. **A Windows machine for real-host loads, pluginval GUI tests and mixed-DPI testing**
   (B427, B432). Windows x64 is a ruled 1.0 platform (B432), but no row names who tests on
   it or on what.
2. **Min-spec hardware for the ×1.5 factor** (B439). Not even a row; B29 asks.
3. **The blade inner loop's efficiency.** This is where most of the per-voice CPU goes
   (§4), and B378 audited only the swarm core. No row owns it.
4. **CPU measured with two oscillators.** B327's 1.0 voice has two SCALPEL oscillators;
   every measurement so far times one engine instance. No row owns it.
5. **Counsel** (B433). It is the human's, and unfunded by the human's word (2026-10-04).

## 4. The CPU-critical item

**The target.** The B439 budget (approved 2026-10-04) splits the E-6 envelope (< 50 % of
one min-spec core, 8 voices, 44.1 kHz, 128-sample blocks; specs/ACCEPTANCE.md E-6) and
gives the sources **≤ 34 %**. It converts this Mac's M3 to min-spec with an assumed ×1.5.

**The record.** `docs/port/h2-engine.md`, checkpoint 4 (B385). These numbers come through
an uncommitted auhost `--engine` patch (B385 H2). They are the median % of real time per
voice at 128-sample blocks, 48 kHz, each preset at its own os, one engine instance per
voice:

| Patch | per voice, 8 voices (M3) | ×8 voices (M3) | ×1.5 → min-spec | vs the 34 % slice |
|---|---|---|---|---|
| Quarter sync (light) | 0.45 % | 3.6 % | 5.4 % | fits |
| Oracle defaults (N 5, sync blade, os 2) | 1.95 % | 15.6 % | 23.4 % | fits |
| Crushed bells (heavy) | 5.68 % | 45.4 % | 68.2 % | **2.0× over** |
| Glass horde pad (heavy) | 8.80 % | 70.4 % | 105.6 % | **3.1× over** |

The last three columns are arithmetic on the recorded numbers, not new measurements.

**Three caveats in the record, all derived and none newly measured:**
- **The machine was loaded.** The doc says so: load average 4–7, and the calibration loop
  took 175–205 ms against 111–119 ms in phase 1a ("read the RATIOS"). If the load
  inflated every figure uniformly, the heavy patches would sit at about 0.54–0.68× of the
  table: Crushed bells 37–46 % and Glass horde pad 57–72 % of min-spec. **Both are still
  over 34 %.** So "over" survives the caveat, and "by how much" (1.1× to 3.1×) does not.
- **48 kHz, not E-6's 44.1 kHz.** Per-second work scales with the rate, so this is about
  0.92× at 44.1 kHz. That is minor.
- **One oscillator.** B327's lineup is two SCALPEL oscillators per voice. If a heavy patch
  runs both, the source cost roughly doubles. *Hypothesis, unmeasured, and no row owns it.*

### The levers

| Lever | Rows | What the record says | Ceiling, derived |
|---|---|---|---|
| **Output-neutral kernel work.** Every parity digest stays unchanged. | B378 F1, F7, F9; B385 | F1 specialised member kernels are bit-identical and 2.0–2.3× faster on the legacy swarm core. F7: the mirrored decimator is 2.67× faster. F9 adds a K = 0 guard. In the composed engine, the swarm adds only **13–25 %** over the blade port (h2-engine.md). | Removing ALL swarm overhead saves at most 11–20 % per voice. **Most of the cost is in the blade render, which no audit has profiled.** That audit has no owner (the gaps list above). |
| **Oversampling defaults** | B380, B383, B357, ADR-189 | The table edges (B383 options d and f) read −120 dB at 1× on every note. The 2-point polyBLEP with 2× oversampling costs ×4–6 and still folds from C6. Option (d) costs ×1.07–×1.32 of plain polyBLEP from C1 to C4, and ×4.7 at C8. The blade carriers (sync, ring, fold) are a separate question (ADR-189 D1–D3, B357's decimator). | If a heavy preset can drop from os 2 to os 1 under the table edge, its per-sample work could fall toward half. *Hypothesis: it is unmeasured on the composed engine.* |
| **Polyphony and quality policy** | B323, B372, B375, B439 | The lab's cap policy is REFUSE by default (B375). Thinning swarm members on heavy presets cut the lab worklet's load, for example Glass horde pad N 9 → 6 took 5 voices from 126 % to 82 % of the block budget (B375, JS). The bar's oversampling exception is a model: a user may choose the expensive mode, but a preset never imposes it. | A per-patch deterministic voice count or member budget can fit any patch, at a cost in sound. That is a ruling (B3), and it must stay deterministic (B323). |
| **The conversion factor** | B439 | The ×1.5 factor is an assumption. The Intel min-spec may be slower than an M1 per core. | Unknown in both directions (B29). |

### The plan to close the gap (proposed)

1. **Measure it properly first. This needs no new design.**
   - Commit auhost's `--engine` mode after #886 (B385, B404).
   - Re-run checkpoint 4 on an unloaded machine, with the calibration loop inside its
     phase 1a band, at 44.1 kHz.
   - Add three cases: 8 voices of BOTH oscillators on the heavy presets; the full 83-preset
     distribution at 8 voices; and the os each heavy preset actually runs.
   - Report the gap per preset against 34 % ÷ 1.5 ≈ 22.7 % of an M3 core.
2. **Profile the blade render**, the 80–89 % that is not swarm. Then do the output-neutral
   kernel work on `h2/engine/`:
   - B378 F1, F7 and F9 ported across;
   - whatever the blade profile finds.
   - Each step keeps every parity digest unchanged, and each is measured against the
     step 1 table. This needs a new row, or a widening of B378's scope: the lead's call.
3. **Rule the edge correction (B2), then the oversampling defaults.** Once a table edge
   is chosen, measure each heavy preset at os 1 against os 2. The default drops only where
   B346's estimator and the human's ear accept it. Each drop is a ledgered divergence
   (ADR-187 A1).
4. **Set the polyphony and quality policy (B3).** The proposed shape mirrors the
   oversampling exception:
   - every factory preset fits the slice at 8 voices on min-spec, or declares a lower
     deterministic voice count (or member count);
   - a user may push past it;
   - the cap policy stays REFUSE with STEAL as an option (B375).
5. **Measure the ×1.5 factor once on real min-spec hardware (B29).** Until then, every
   cost row keeps the "assumed" label (B439).

Step 1 decides how much the rest must deliver. **CPU stays Layer-E: measured and reported,
never a verify gate (ADR-187 §8).** The CPU item is a 1.0 gate under the bar (row 4), not a
feature-freeze gate. It does not move the freeze date.

**FX-side notes**, for the same budget:
- The legacy rack modules are cheap. Room is the worst at 0.67 % (B262).
- MAW is projected at ~9.7 % per instance (3 stages, 8× oversampling). It fits only under
  the oversampling exception, and only with a cheaper mode (B439).
- Sluice's own 1× figure may be over the per-module line (B28).

## 5. The proposed freeze date: Wednesday 2026-10-07

**Why 10-07, and not 10-06:**
- **The roster decisions (group A) are all the human's own calls.** None waits on a
  sibling's reply, a lab build or a measurement.
- **The human has been ruling at that rate.** On 2026-10-04 alone: B425, B431, B432, B433,
  B435, B438, B439 and the budget.
- **Day 1 (10-05):** this proposal and the records PR #926 land.
- **Day 2 (10-06):** one review session for A1–A12.
- **Day 3 (10-07):** the lead records the rulings and declares the freeze in DECISIONS.
- 10-06 would leave no slack if A1, the bar's timing, needs discussion. A1 is the one
  decision without which "freeze" is ambiguous.

**What would push it later:**
- **A1 not ruled.** The freeze cannot be declared while the bar says every MUST holds by
  it. Under that reading, nothing qualifies, and the Sub, which is in the 1.0 lineup,
  would have to be dropped.
- **A2 is the safety valve.** If the human approves "undecided means post-1.0", any other
  group-A item still open on 10-07 simply goes post-1.0, and the date holds.

**What the freeze does NOT need, and why that is safe:**
- **Scope lists.** They freeze at each module's lab sync under A1, so the Sub's workshop,
  the OTT lab, Scape's spec and the B408 stereo lab do not have to finish by 10-07.
- **The CPU plan.** It is a bar gate, not a freeze gate.
- **B427–B433.** They are production work, not features.

**Comfort check.** The outcome this pass hoped for was "freeze on 10-06 or 10-07". That
is the conclusion it reached, so it is the one to examine hardest. The examination is the
A1 finding. The bar, read as written, makes any near-term freeze empty, so the date is
sound only if A1 is ruled as proposed. If the human means the bar literally, then no date
within weeks is honest, and the right answer is a roster lock with no "freeze" label.

## Sources consulted

- ROADMAP.md at `origin/lead-records-181`: rows B384–B440, plus B50, B101, B126, B152,
  B210, B225, B234, B236, B258, B260, B262, B263–B269, B274, B275, B277, B290, B303,
  B308, B318, B322, B323, B327, B328, B331, B336, B346, B350, B355, B357, B366, B369,
  B370, B372, B373, B375–B383.
- `docs/H2-PLAN.md` (last verified 2026-10-01), `docs/proposals/module-1.0-bar.md`,
  `docs/proposals/module-io-gain.md`, `docs/port/h2-engine.md` (CPU, checkpoint 4),
  specs/ACCEPTANCE.md E-6.
- Sibling READMEs, read only: Bulwark (status table), Scape (status table), Maw
  (status), Sluice (README; its ROADMAP Q-024; its DECISIONS D-096 and D-108).
