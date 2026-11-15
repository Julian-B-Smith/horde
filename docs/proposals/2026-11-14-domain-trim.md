# Proposal — a trimmed CLAUDE.md §Domain

**Origin.** horde lead session, 2026-11-14; ADR-398 (the human's B676 rulings on the 2026-11-10
audit, `docs/audits/2026-11-10-fable-audit.md`, item 13 and the weekly-audit cadence).

**What this is.** A complete replacement for the `## §Domain — HYPERSAW` section of the root
`CLAUDE.md` (lines 73–203 at origin/main e4b0cebc, from the heading up to the
`<!-- KNOWLEDGE-LOOP:START -->` marker that precedes `## Self-Improving Knowledge Loop`). The
audit found that section 12.9 KB long, loaded into every session, mostly candidate history, and
in parts stale since ADR-387 made wavetables replace blades for 1.0 with three oscillators
(`docs/audits/2026-11-10-fable-audit.md:321-327`; flagged before in
`docs/audits/2026-10-10-repo-audit.md:972-990`, `2026-10-21-repo-audit.md:47`,
`2026-10-29-repo-audit.md:368-371`). Every rule is kept in meaning; the history moves to where it
already lives; the stale facts are corrected against the tree or dropped.

**CLAUDE.md is the human's file; nothing changes until the human approves.** The text below is
meant to be approved word for word, or edited, and then pasted over lines 73–203.

The `## Mailbox` section is **outside** §Domain (it sits after the knowledge loop, inside the
`kit:2.1.0` markers, `CLAUDE.md:237-261`), so it is untouched by this proposal.

## (b) The proposed §Domain

````markdown
## §Domain — HYPERSAW

**What this is.** horde (the device, ADR-114): a CLAP-native instrument whose centre is morph
logic over the full depth of every parameter (ADR-337). One unified engine (GUI-1): every
oscillator is a swarm (a Kuramoto-coupled oscillator population) whose members read wavetables,
and 1.0 has three oscillators (ADR-387); nothing offers swarm and SCALPEL as engines to choose
between. Two products (ADR-186): the legacy plugin in `src/`, frozen; and horde 2, whose engine
is rebuilt from `specs/SPEC-ENGINE.md` by a builder in the separate clean-room repos (ADR-223,
ADR-224, ADR-247; the clean-room rule is in `engine2/README.md`), with the wavetable member specified in
`specs/SPEC-WAVETABLE.md` (ADR-385); `engine2/` holds the builder's scaffolding and input data.
HYPERSAW is the swarm engine's old name (ADR-115; it is SWARM SAW since), the frozen legacy
plugin id, and our name in sibling mailboxes. Earlier engine, module and FX candidates are
history: DECISIONS.md and ROADMAP.md by ADR and B number, and `docs/PARKED.md`.

**Stack & entrypoints.** C++20, CLAP-first: plugin shell `src/hypersaw_clap.cpp` (the legacy
shell; horde 2 has no shell in the tree yet), wrapped to VST3/AUv2 by clap-wrapper (ADR-002;
`libs/` pins clap 1.2.10, clap-wrapper v0.15.1, and choc, patched per ADR-196). Build: `cmake -S
. -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release` then `cmake --build
<abs>/build-release -j` (absolute path — sandbox resets cwd). Cores: `src/*_core.h` (legacy),
`h2/` (horde 2's composed engine, which stays until the clean-room engine replaces it; ADR-223
item 6). `reference/` holds the spec-in-code prototypes (ADR-003).

**Domain invariants** (the critic checks against these):
- The DSP core is deterministic code; no LLM judgment in the runtime path. Same seed + note order
  → identical output; mulberry32 streams only; no wall-clock reads anywhere in the core (SPEC §5.7).
- C++ correctness. Legacy shell: parity with the JS reference (L0-1, ε=1e-6 RMS) plus the L0
  trajectory criteria, never plausible-sounding audio; any intentional divergence from the
  prototypes requires an ADR. horde 2's cores (ADR-187): the core's JS parity target on every
  scenario no ratified divergence claims, and, once its JS is demoted by ruling, its frozen
  goldens, divergence ledger and Layer-0 invariant suite; divergences are mirrored into the lab JS
  where possible, and `js_limit` marks where the browser cannot follow and never slows the C++. The
  new engine (ADR-224, B494): no JS parity; its ratified spec (`specs/SPEC-ENGINE.md`), its Layer-0
  invariant suite and the human's ears on the bank. The current composed engine stays under
  ADR-187 until replaced.
- Slew/time-constant math is in seconds, converted to per-tick coefficients (16-sample tick =
  2756/s at 44.1 kHz); hand-tuned per-tick constants are banned (ADR-009).
- The real-time thread allocates nothing and calls no external provider.
- Acceptance numbers in specs/ACCEPTANCE.md are measured, not aspirational; they change only with
  a re-measurement on the reference implementation.

**Numbers** (ADR-346). Each writing session numbers on its own counter: the lead's rulings are
`ADR-n` and its rows `Bn`; the GUI session's are `GUI-n` and `GBn`. A new parallel session's prefix
pair is registered in `DOMAINS` in `tools/design_chain_check.py` in the PR that charters it; an
unregistered prefix is red. All rulings stay in one `DECISIONS.md`; existing numbers never change.

**Gate wiring** (ADR-180 §1). The gate on `./verify` is a gate on WEAKENING a check (removing,
skipping, relaxing, excluding). ADDING one is not gated: a new `tools/*_check` is wired into
`./verify` in the PR that creates it, or carries `UNWIRED: <reason>` in its header;
`test_table_check` enforces wired-or-explained and prints the wired count every run.

**Blind-Spot Armor** (ADR-197, B448). `docs/strategy/blind-spot-armor.md` is law: every
risk-register row is a named gate in `./verify` or a visible hole on the dashboard; the
agent-signature failure modes are the reviewer's checklist; output guards latch and report, never
clamp silently; every default change carries an ADR AND a migration; the human's ears get a capped
listening batch, not interruptions. Correctness and security share one armor catalogue and
dashboard.

**Auditor cadence** (B159, ADR-179 §3; paused by ADR-398). The weekly repo audit is PAUSED while
any carried finding (B683 to B690) is neither DONE nor WONTDO; meanwhile the lead does not
dispatch `.claude/agents/auditor.md` at session open. When every one is DONE or WONTDO, the lead records it in
DECISIONS and the 7-day cadence resumes: at the first session open of a working day with no repo audit
in `docs/audits/` newer than seven days, the lead dispatches the auditor in the background; its
report is its own PR and its findings become ROADMAP rows at the next boundary.

**Alias note.** This repo is public. Never write a private sibling's real name into a tracked
file; use its alias ("terrain sibling", …). The alias→name map is `PRIVATE-NOTES.md` (untracked,
local-only — ADR-014).

**Protected paths** (human gate to modify; the references ARE the reference — an edit there is a
spec change):
- `specs/`: `SPEC.md`, `SPEC-EFFECTS.md`, `SPEC-SWARMALATOR.md`, `SPEC-FORMANT.md`,
  `SPEC-DISTORTION.md`, `SPEC-STATION.md`, `SPEC-INTENT-BUS.md`, `SPEC-ORBITAL.md`,
  `SPEC-FX-NETWORK.md`, `SPEC-MODULE-MACROS.md`, `SPEC-MAW.md`, `SPEC-SUBOSC.md`,
  `SPEC-SCALPEL.md`, `SPEC-QUADRANT.md`, `ACCEPTANCE.md`; and `docs/PRIOR-ART.md`.
- `reference/`: `swarmsaw.html`, `swarmspectra.html`, `swarmdynamics.html`, `swarmfilter.html`,
  `swarmphaser.html`, `swarmtime.html`, `swarmalator.html`, `formant-pulsar-fof.html`,
  `decoherence-lab.html`, `station.html`, `intent-bus.html`, `gravity-modulator.html`,
  `network-lab-v0.html`, `horde-module-macros.html`, `subosc.html`, `maw/**`, `scalpel/**`,
  `quadrant/**`, `tablecaster/**`.
- `docs/design-system/` (ADR-261): `HORDE-Morph-Knob-Spec.md`, `HORDE-Map-Mode-Spec.md`,
  `HORDE-Morph-Knobs.html`, `HORDE-Map-Mode.html`, `HORDE-Slider-Marks.html`.
- `./verify`; the golden render fixtures.

Open sanctions: seed `network-lab-v0.html`'s four draws (ADR-166) and `quantumPick` (ADR-169).
SCALPEL's seed edit is unsanctioned (ADR-184); every other is spent.

**Verify targets.** `fast`: leak gate and minutes-scale checks; CI-blocking. `full`: fast + Release
build, parity and trajectory oracles, Layer-E checks with human sign-off; macOS-local (auval and
host loads are human-run). `nightly`: long jobs (ADR-209).
````

## (c) What moved or changed

Line numbers are `CLAUDE.md` at e4b0cebc.

**Moved out (history; it already lives elsewhere).**
1. **The candidate ingest log** (`:77-111`: SPECTRA, the formant engine CANTO, WARP, STATION,
   ORBITAL, NETWORK, MAW, module macro tiers, SUB OSC, SCALPEL, and "the family described next is
   how it got here"). Replaced by one sentence pointing at DECISIONS.md and ROADMAP.md by ADR and
   B number and at `docs/PARKED.md`. Each candidate's record: SPECTRA and CANTO and the swarmalator
   archived (`docs/PARKED.md:31`, entry 23); WARP parked (ADR-092 amendment); STATION out of 1.0
   (ADR-182, `docs/PARKED.md:29`, entry 22); ORBITAL ADR-165; NETWORK ADR-166; MAW ADR-170; module
   macros ADR-169; SUB OSC ADR-178; SCALPEL ADR-184, now history for 1.0 (ADR-387, its "What
   follows"); QUADRANT ADR-384; TABLECASTER ADR-386. Their files stay protected (the list is
   unchanged, below).
2. **"A synthesizer built from a family of experimental, responsive engines …"** and the
   superlative for SWARM SAW that follows it (`:78-82`). GUI-1 asked for exactly this wording to
   go (GUI-1, "Needed from the lead"), and `tools/engine_words_check.py:28` refuses the same
   phrase in the master and the shell. The device description is now ADR-337 + GUI-1 + ADR-387.
   SWARM SAW's naming history stays in ADR-115.
3. **Per-file ingest dates, ADR notes and spent sanctions** in the protected-path paragraph
   (`:175-197`). The paths are kept (item 16); the dates and "sanction spent" notes live in each
   file's ADR (122, 152, 165, 178, 184, 261, 310, 384, 386). The still-open sanctions are kept.
4. **"The same step is proposed to the kit's `/wakeup` (autonomous brief hypersaw-003)"**
   (`:168`). History of ADR-179 §3; moot while the audit is paused.
5. **Ratification dates on Numbers, Gate wiring, Blind-Spot Armor and the ADR-187/224 scopings,
   "folder renamed 2026-10-04, B437", "settled 2026-08-23"** (`:111-115`, `:144`, `:150`,
   `:162-166`). The ADR carries the date.
6. **ADR-337 A1's positioning** ("discovery instrument … professional fidelity"), considered for
   the new "What this is" and left out for size; it lives in ADR-337 A1.

**Changed (the ruling).**
7. **Auditor cadence** (`:168`, B159, ADR-179 §3) is paused by ADR-398, not replaced. ADR-398
   (§"ADR-398 — RULED by the human: the 2026-11-10 audit's seven unruled findings", uncommitted
   in this PR's `DECISIONS.md`) rules, in item 7:

   > **The audit cadence: paused until the carried list is empty.** The carried list is B683 to
   > B690. While any of them is neither DONE nor WONTDO, the lead does not dispatch the weekly
   > auditor at session open. When the list empties, the lead records it in DECISIONS and the
   > 7-day cadence resumes. CLAUDE.md's "Auditor cadence" paragraph is the human's; its new
   > wording rides in B687's §Domain proposal, and this ADR governs until then.

   The new paragraph states that pause and keeps ADR-179 §3's running mechanics in one clause
   (first session open of a working day, no repo audit in `docs/audits/` newer than seven days,
   dispatched in the background, report as its own PR, findings become ROADMAP rows at the next
   boundary), so the
   cadence that resumes is still defined. `.claude/agents/auditor.md` exists. ADR-398 rides on
   the same PR as this proposal.

**Changed (stale, corrected against the tree).**
8. **"every oscillator is a swarm whose members read wavetables (ADR-387 …)"** (`:75-77`) kept,
   now first; "the plugin has three oscillators in 1.0" from ADR-387 item 2. The swarm's physics
   is kept in five words ("a Kuramoto-coupled oscillator population", from `:80-82`): the
   invariants assume the reader knows what a swarm is, and ADR-387 keeps the swarm as the
   oscillator's core.
9. **The two products** (new sentence): the legacy plugin in `src/` is frozen and horde 2's engine
   is rebuilt from `specs/SPEC-ENGINE.md` (ADR-186; ADR-223; `h2/README.md:7`). The builder works
   in the separate clean-room repos (ADR-247 and later ADRs); in this repo `engine2/` is scaffolding and input data, and its
   `build.sh` builds an empty library and prints `suite: 0 items` (`engine2/README.md:3-5`,
   `:20-22`), so it is not listed as a build entry. The wavetable member's spec is
   `specs/SPEC-WAVETABLE.md` (ADR-385). §Domain never named `h2/` or `engine2/`
   (`docs/audits/2026-10-29-repo-audit.md:370`).
10. **`libs/` pins** (`:120`): choc added as the third submodule, patched (`.gitmodules:7-9`;
    `git submodule status` reads clap 1.2.10 and choc a08bfd8, and
    `git -C libs/clap-wrapper tag --points-at HEAD` reads v0.15.1; ADR-196). Not a new
    dependency: it was already pinned, only unlisted.
11. **"Reference implementation: seven single-file HTML prototypes plus one CANDIDATE … Ported C++
    cores so far: …"** (`:123-133`). False: `reference/` holds 16 HTML files and four packet
    directories (`ls reference`), `src/` holds 17 `*_core.h` (`ls src/*_core.h | wc -l`), and
    `h2/` holds horde 2's cores (`h2/README.md:12-17`). Replaced by "Cores: `src/*_core.h`
    (legacy), `h2/` (horde 2). `reference/` holds the spec-in-code prototypes (ADR-003)." "impl
    `src/hypersaw_clap.cpp`" becomes "plugin shell … (the legacy shell; horde 2 has no shell in
    the tree yet)": CMake builds only two shells, this one and the parked SWARM-FX
    (`CMakeLists.txt:176`, `:306`), and `h2/` holds no CLAP entry.
12. **"The E1 effects ship today only in a SECOND CLAP shell, `src/swarmfx_clap.cpp`"** (`:133`).
    Half stale: the rack carries `notch_core.h` (`src/fx_rack.h:30`); the E1 resonator bank,
    `filter_core.h`, still ships only in SWARM-FX (`src/swarmfx_clap.cpp:23`, its only include in
    `src/`). SWARM-FX is parked, still built (`CMakeLists.txt:306-313`); its status lives in
    `docs/PARKED.md:27` (entry 21), so the sentence is dropped rather than rewritten. Its revisit
    trigger ("E1 folds into the rack") cannot fire (`docs/audits/2026-10-10-repo-audit.md:1006`);
    that is for PARKED.md, not this section.
13. **"Build/test = `./verify fast|full` (the gate list is `verify`'s own header …)"** (`:133`).
    `verify`'s header states its contract, not its gates (`verify:1-4`). The test_table_check
    count clause moves into Gate wiring, where it is true: the tool prints
    `test_table_check: GREEN (N tests — …)` (`tools/test_table_check.py:266`).
14. **"This Mac: Command Line Tools only … see the global CLAUDE.md audio-plugin section"**
    (`:133-135`). Dropped: the global file is loaded in every session already and holds the same
    facts; the absolute-path gotcha stays beside the build command.
15. **"`HYPERSAW` is the … ENGINE"** (`:113`). ADR-115 renamed the engine SWARM SAW; HYPERSAW is
    now "the swarm engine's old name (ADR-115; it is SWARM SAW since)", the frozen plugin id
    (`src/plugin_identity.h:33-35`) and our mailbox name; the GUI-1 wording is not carried
    over (item 2). "Design docs: …" (`:115-116`) dropped: SPEC.md, ACCEPTANCE.md and PRIOR-ART.md
    are in the protected list, PARKED.md in the history sentence.
16. **Protected paths: "the prototype HTMLs — now seven"** (`:176-177`) went on to name more than
    seven; the count is dropped and every file named. The list is restructured, not changed: 42
    entries before and after (16 specs/docs, 19 references, 5 GUI 3 files, `./verify`, golden
    render fixtures), checked name by name. The three GUI 3 prototypes are given their folder,
    `docs/design-system/` (`ls docs/design-system`). Other file names mentioned in the section
    changed as listed in items 9, 11 and 12. "golden render fixtures once they exist" becomes
    "the golden render fixtures": they exist (`docs/audits/2026-10-10-repo-audit.md:989`), but
    naming them is left to the human (Open, below).
17. **Open sanctions** kept, verified still unspent: `reference/network-lab-v0.html` has four
    `Math.random()` draws (`:458`, `:743`, `:987`, `:994`) and `reference/horde-module-macros.html`
    one, the `quantumPick` draw (`:497`). SCALPEL's seed edit was never sanctioned (ADR-184).
18. **Verify targets** (`:199-202`). "`fast`: leak gate + structure/manifest sanity now; grows
    the L0 suite (parity + trajectories) from Phase 1" is false: the parity and trajectory
    oracles run in `full` (`verify:1014-1026`), after the Release build (`verify:977-987`).
    `fast` is minutes, not seconds (B672: 106–124 s) and CI-blocking
    (`.github/workflows/ci.yml:117`). "pluginval … not CI-able here" is false: CI runs pluginval
    (`.github/workflows/ci.yml:19-31`); auval and host loads stay human-run. `nightly` added, as
    `verify` dispatches it (`verify:1754-1756`, ADR-209 item 4).

**Unchanged in meaning, reworded for size.** The domain invariants (the parity bullet regrouped
by shell: legacy, horde 2's cores under ADR-187, the new engine under ADR-224; the doctrine
parenthetical on the provider dropped), Numbers, Gate wiring ("the charter's gate" → "the gate"),
Blind-Spot Armor (verbatim but for the date), the alias note (the rule first, then the alias and
the map).

## (d) Size

Measured with `wc -c` on the extracted text (the proposal's fenced block, and `sed -n '73,203p'
CLAUDE.md` at e4b0cebc):

| | bytes |
|---|---|
| before (`CLAUDE.md:73-203`) | 12858 |
| after (the fenced block above) | 6475 |

## Open for the human

- `engine2/CLEANROOM.md` and `tools/engine2_cleanroom.sh` still describe a sparse worktree of this repo, not the separate clean-room repos the builder uses since ADR-247; that needs a follow-up row (not part of this proposal).

- **Golden-fixture paths (B686 needs files to hash).** ADR-398 item 3 builds
  `docs/armor/protected.json` from this list as it stands on 2026-11-14, with globs expanded to
  files (B686); "the golden render fixtures" names no file, so the ledger cannot hash it. The
  candidates the 2026-10-10 audit found (`docs/audits/2026-10-10-repo-audit.md:989`):
  `tests/state_fixtures/`, `tests/morph_order.txt`, `h2/engine/selfdigest.*.txt` (the selfdigest
  files call themselves "a golden fixture (protected)" on their line 2). Naming them adds paths to
  the list, so it is the human's call: name these, name others, or leave the entry for B686 to
  resolve.
- **Two stale incoming pointers** to §Domain, before and after this trim; each needs a follow-up
  row, not text added here without a ruling: `.github/workflows/ci.yml:19-22` calls the Windows
  WebView2 GUI runtime "an explicitly-deferred residual (see CLAUDE.md §Domain)", which §Domain
  never held; `docs/design/kuramoto-lfo-prototypes/NON-GOLDEN.md:9` calls the protected set "the
  seven listed in `CLAUDE.md §Domain`", a count this trim removes.
- **The protected list feeds the hash ledger.** Item 12 of the 2026-10-29 audit is ruled
  (ADR-398 item 3, B686): `docs/armor/protected.json` is built from this list as of 2026-11-14.
  Once it lands, the Protected paths paragraph can point at it instead of listing files by hand.
- **`reference/distortion-engine.html`** (WARP's prototype) was never in the protected list,
  though its spec `specs/SPEC-DISTORTION.md` is; `specs/SPEC-GLITCH-MODULES.md` is named only as
  the thing `decoherence-lab.html` is the reference of. Both are left as they are.
- **"with human sign-off"** for `full` is kept from the original; `verify` itself does not record
  a sign-off, so it reads as process, not as a gate.
