# The horde 2 shell acceptance list (B398; armor phase 2, package P17)

> **Origin.** horde lead session, brief dated 2026-10-11, for ROADMAP B448 (Blind-Spot Armor),
> phase 2 Wave 1, package P17 of `docs/strategy/blind-spot-armor-phase2.md`. The human ruled the
> phase in ADR-209. An implementer drafted this page from that brief.

**Status: DRAFT, for the human to ratify with B398.** Nothing here is ruled except where a row
cites a ruling. Last verified 2026-10-10 against `origin/main` at `27047c2`.

**What this is.** The horde 2 plugin shell does not exist yet (B398; `docs/H2-PLAN.md` step A2).
ADR-209 item 1 adopted the rule "engine now, shell at birth, legacy frozen": a risk that lives in
the shell gets no new gate on the legacy shell. It is written down now and built with the new
shell's skeleton. This page is that list. It is what the shell's author is held to.

**What this is not.** It is not code, and it adds no gate today. It does not restate shell rows
that other ROADMAP rows already own (section 7). It does not cover the engine-level gates of
packages P2 to P16.

## 0. How to read it

One row per parked gap. Each row gives the gap, a gate, a control, where it runs, the legacy
check that is its template, and its source.

- **Gate names are suggestions**, as in the phase 2 proposal. A gate that is a `tools/*_check` is
  wired into `./verify` in the PR that creates it, with a `WIRED:` header (ADR-180 §1).
- **A control is the planted fault that proves the gate can go red.** A gate whose control passes
  is red. Where no control is known, the row says so.
- **Where it runs:**

| Mark | Meaning |
|---|---|
| fast | `./verify fast`: no build tree, seconds, on every PR |
| full | `./verify full`: the dev Mac, before an item is called done |
| nightly | `./verify nightly`, one scheduled GitHub workflow; green for 48 hours (ADR-209 item 4) |
| CI job | A workflow job outside `./verify`, such as `validate-windows`. Workflow edits are the human's push |
| tag | The release jobs that run on a version tag |
| hands | The human, in a real host. A recorded pass, never a script |

- **Sanitizer gates** build with Homebrew LLVM. Without it they print SKIPPED and never pass, and
  the dashboard shows the skip as a hole (ADR-199).
- **Lifting.** A legacy header the shell needs is copied into `h2/` in horde 2's own namespace,
  with a lift ledger entry. It is never shared (ADR-186 §4; `h2/README.md` rules 1 and 8).
- **Respected limits** (proposal §5): no stopwatch and no language model in `./verify`; no
  saved-state corpus before the first release; no scripted DAW matrix; bit-exact digests on one
  platform only, tolerance elsewhere; no new gate on the legacy shell.

## 1. Realtime and threads (armor rows 1 and 2)

| # | Gap | Gate, and what it checks | Must-fail control | Runs | Legacy template, and what changes | Source |
|---|---|---|---|---|---|---|
| RT-1 | The realtime probe covers the legacy `process()` only. horde 2 has no shell to probe. | `h2_shell_rtsan_check`. RealtimeSanitizer over the shell's audio-thread entry points: process, params flush, start and stop processing, reset. The seeded schedule reaches every lockfile parameter id, loads staged on the main thread and drained on the audio thread, bypass and reactivation. Zero violations. | One `malloc` planted in the realtime scope must be reported. The same scope, empty, must be clean. | full; nightly on a macOS runner if one is available | `rtsan_check` with `tools/rtsan_probe.cpp`. The probe includes the new shell's source. Its id list is read from the lockfile (SC-1), not written by hand. "Paths outside the seeded schedule" needs a dated accepted limit before the row can be green (ADR-209 item 2). | Catalogue R1; ADR-199; proposal §1 row R1 |
| RT-2 | A function-local static reached from render takes a lock and builds its tables on the audio thread at first use. The lifted swarm core has one (`h2/cores/swarm/swarm_core.h:118`). | `h2_static_warm_check`, a row group of RT-1. Every function-local static reached from render is warmed at activate, on the main thread. The first block of a fresh process shows zero violations. | A build with the warm call removed must report the first-use lock. It runs in its own process: a static warms once, so a second run in the same process cannot fail. | full; nightly | `rtsan_check`, which found this on legacy (`src/swarm_core.h:118`, fixed in `plug_activate`). The core is not edited; the shell warms it. | ADR-200 shell rule 6; B448 item B3 (PR #999) |
| RT-3 | No lint of the realtime ban list: `new` and `delete`, mutexes, `std::function` capture, container growth. | `h2_rt_banlist_check`, the lint package P16 builds over `h2/`, extended to the shell's audio-path files. | A planted container growth in a shell audio-path file is red. A shell source file outside the lint's file set is red. | fast | None on legacy. `banned_api_check` has the same shape but a different list. | Armor row 1; catalogue R1; proposal P16 |
| RT-4 | The allocation counter replaces only the plain `operator new` forms (`tools/rtsafety_probe.cpp:44-55`), and no planted fault was found in it by search. | `h2_shell_rtsafety_probe`. Every allocation form (plain, array, nothrow, aligned) is counted while `process()` runs, through the real CLAP path. Zero. Every path that ships is inside the armed window. | One planted allocation of each form inside `process()` must be counted. | full | `rtsafety_probe`. It gains the missing forms and a plant. It is one of the 55 shell-linked tools horde 2 must re-earn (B308 H5). | Armor row 1; B430 item 7 |
| RT-5 | The thread-race harness is legacy only, and it accepts read-only races until the park trigger. | `h2_shell_tsan_stress_check`. ThreadSanitizer while a host-like main thread drives the real entry points during render: parameter writes, state save and load, preset apply, morph, history, editor polling. Three seeds. Zero reports of any class. | A planted main-thread write must be reported and filed as a write. A planted main-thread read must be reported and must FAIL. The audio-only run must be clean. | full; nightly on a macOS runner if one is available | `tsan_stress_check` with `tools/tsan_stress.cpp`. The read class moves from accepted to failing, because the shell's main thread never reads audio-owned memory. The `halt_on_error=0` approval was given for the legacy harness; a copy raises the weakening count and needs its own approval. | ADR-200 gates and shell rules 1, 2 and 7; catalogue R2; proposal §1 row R2 |
| RT-6 | Edits must travel as typed commands on bounded channels that report overflow. Nothing checks the new shell's channels. | `h2_shell_handoff_check`, overflow rows. Each command channel is overfilled inside one block. The overflow is counted and readable. A load that overflows still lands whole. A deferred event with an impossible size is refused and counted before any copy. | A build that drops an overflowing entry without counting it is red. A build that copies before checking the size is red. | full | `load_handoff_check` rows `O-*` and `D-*`. Each new channel states its bound, as legacy records its 4096-entry queue and 16 KB event buffer. | ADR-200 items 4 and 7, shell rule 3 |
| RT-7 | "One writer per state object" and "no cross-thread guard flags" have no static check. A check-then-act on an atomic is race-free to ThreadSanitizer however wrong its order is. | The seam registry gate of package P19: a shell file with threading or shared state must belong to a registered seam. | A new atomic in an unregistered shell file is red (P19's control). **No complete control is known for "guard flag".** The ordering rows of HB-1 catch the cases written down; a new flag is caught only by the registry row and by review. | fast | None. `load_handoff_check`'s header records why a sanitizer cannot see ordering. | ADR-200 shell rules 1 and 5; proposal P19 and §1 row S7; B275 |
| RT-8 | GUI activity and the audio thread have never been stressed together under a deadline measure. | `h2_shell_gui_load_check`. Notes rendered while the editor floods its bridge are bit-identical to the same notes with no editor activity. | A planted editor write that bypasses the command queue changes the render and must be red. | full | None. This row uses bit-identity, not time: a wall-clock deadline cannot sit in `./verify` (proposal §5, citing ADR-187 §8). See open question 8. | B430 item 2; B449 (rule: notes are bit-identical under GUI load) |

## 2. Host boundary and state loads

| # | Gap | Gate, and what it checks | Must-fail control | Runs | Legacy template, and what changes | Source |
|---|---|---|---|---|---|---|
| HB-1 | A load made while processing must be adopted whole at a block boundary. The oracles that prove it are legacy only. | `h2_shell_handoff_check`, load rows. A queued load equals an idle load. A later load supersedes an earlier staged one. A load is never recorded as an edit. Events that arrive during an excluded block are replayed in order, with a host reset kept in its place. | Each row keeps a must-fail control or a recorded mutation proof, as ADR-200 requires. One named plant: a build that adopts a load across two blocks must be red. | full | `load_handoff_check` (84 rows at ADR-208). Rows that name legacy mechanisms (the morph field, intent tables, engine revision) are restated for horde 2's patch model. | ADR-200 items 3 to 5 and shell rule 4 |
| HB-2 | A load made while the plugin is not processing must be whole when it returns. | Same check, idle rows. A save made at once returns the loaded state. The load supersedes anything queued before it. A save names the patch whose values it writes. | A build that queues an idle load's values, so that an immediate save returns the old state, is red. | full | `load_handoff_check` rows `I-SAVE*`, `I-SUPERSEDE`, `IS-*`. | ADR-208 item 1 |
| HB-3 | The host must be told of a plugin-side load by one parameter rescan, on the main thread, after deferred events replay. On legacy this was verified by reading, not in a host. | Same check, rescan rows: a stub host counts the calls, their flags and their thread. Then by hand in each trial host (VH-7): click a preset while stopped, confirm the values and the modified flag, save, reopen, confirm the preset. | A build that reports each value as a separate change is red. A rescan made from the audio thread is red. A rescan made before the replay is red. | full; hands | `load_handoff_check`'s stub host already counts rescans. The hands-on half is the check ADR-208 item 2 records as owed. | ADR-208 item 2 |
| HB-4 | Host values must be checked where they enter the shell: events, sample rate, output buffers, MIDI bytes, note keys, expressions, velocity, tempo. | `h2_shell_hostile_events_check`, through both doors (process and params flush). An absent or undersized event is refused and counted. Activation is refused for a rate that is non-finite, not positive, below 8 000 Hz or above 768 000 Hz, and a refused plugin writes silence if processed anyway. A missing bus, fewer than two channels or a null buffer returns `CLAP_PROCESS_ERROR`, and the block's events are replayed at the next good block. A MIDI data byte with its top bit set is dropped and counted. | Each row has a mutation proof: the boundary check removed, the row red. One control row shows the hazard is real: a core driven directly with an infinite frequency must render non-finite samples, or the detector is blind. | full; the Linux sanitizer job, if wired in the `"$build_dir/<name>"` shape that `tools/sanitize_oracles.sh` parses | `hostile_events_check` (187 rows at ADR-207). The boundary helpers in `src/input_guards.h` are lifted, not shared. New entry surfaces get rows as they land. Output for valid input stays bit-identical. | ADR-207 item 2; armor rule D (boundary validation is required, not symptom clamping) |
| HB-5 | State and preset files are untrusted input, and nothing fuzzes a parser. | `h2_state_fuzz_check`. Seeded mutations of valid states and presets (truncated, bit-flipped, huge, deeply nested, wrong version, duplicate keys, non-finite numbers) go into the shell's loader under AddressSanitizer and UBSan. No crash and no sanitizer report. After a refused load the instance holds exactly its prior state, never half of the new one, and its output is finite. | A parser fault planted behind a fault flag (an unchecked length) must be found inside the PR-size run. A planted half-applied load must be red. Every failing seed is kept as a fixed row. | fast for the kept seeds; full for the seeded run; nightly for the deep run | None: `statefix_check`, `state_check` and `paste_cap_check` are hand-written cases. Seeds come from the shell's own saver, because no saved-state corpus exists before the first release. The fuzz engine is open question 9. | Armor ("the preset parser gets fuzzed") and phasing 3; B430 item 5; B446 (a fuzz harness on state and preset loading) |
| HB-6 | horde 2 owes nothing to legacy DAW state, and a legacy importer may never sit in the loader. | A fixed row of HB-5: a legacy state chunk and a legacy preset file given to horde 2's loader are refused whole, with the reason reported. | A loader that accepts any field of a legacy chunk is red. | fast | None. Legacy presets reach horde 2 only through the offline porter (B312). | ADR-186 §3 and critic recommendation (ii); ADR-197 answers (a clean break) |

## 3. Output guard and denormals (armor row 3)

ADR-209 item 7 cites "ADR-186 §4" for the guard. That section is the copy-forward rule. The guard
itself is defined by armor row 3 with edit D (ADR-197) and by `src/output_latch.h`.

| # | Gap | Gate, and what it checks | Must-fail control | Runs | Legacy template, and what changes | Source |
|---|---|---|---|---|---|---|
| OG-1 | horde 2's engine has no output guard. It ends in `tanh`, and a NaN passes through (`h2/engine/engine.h:1961`). | `h2_nan_latch_check`. The shell's guard zeroes NaN, +Inf and −Inf, counts samples and blocks exactly, and latches until reset. Finite samples, including −0.0, a denormal and `FLT_MAX`, are bit-identical afterwards. | Five faulty guards must each fail a row: zero-without-count, count-without-latch, a self-clearing latch, one channel unreported, a reset that keeps the latch. | fast | `nan_latch_check` with `src/output_latch.h`. The header and `input_guards.h` are lifted. The "one channel unreported" control covers every output channel the shell has. | ADR-209 item 7; armor row 3 and edit D (ADR-197); B430 item 1 |
| OG-2 | A guard that exists but is not the last line before the host's bus protects nothing behind it. | Rows in `h2_shell_hostile_events_check`: through the real `process()`, a NaN planted by a fault hook reaches the bus as zero, and the plugin's own counter reads it. The guard runs after the last stage that can write the bus. | A NaN planted in the last stage, in a build with the guard moved one stage earlier, must reach the bus and turn the row red. | full | `hostile_events_check`'s six LATCH rows, which read the real plugin's counter. | ADR-209 item 7; catalogue R3; `src/output_latch.h` header |
| OG-3 | The guard's count reaches no user. | `h2_guard_count_gui_check`. After a planted NaN the count the editor shows equals the latch's count. It stays shown through later clean blocks. | A bridge that always answers zero is red. A display that clears itself on a clean block is red. | full on macOS; hands on Windows (VH-8), where CI opens no editor | None for the count. `gui_webview_check`'s run-time rows are the mechanism. Where it is shown and what clears it are open question 7. | ADR-209 item 7; catalogue R3; proposal §2 ("never on legacy") |
| OG-4 | Flush-to-zero is ruled ON (ADR-211 item 1) and round-to-nearest is forced inside the engine (ADR-213). Every gate that replays the engine runs it with the mode on over the whole replay, set in one place (`h2engine_stream::replay`), and the self-digests pin those bits. No shell exists to set it. | `h2_ftz_scope_check`. Every shell entry that runs engine code runs it inside `horde2::underFlushToZero` (`h2/flush_to_zero.h`, the copy the tools' replay uses): the audio-thread entries (process, params flush, start and stop processing, reset) and every main-thread site that constructs, seeds or sets the engine. Inside each, the mode reads back as on, and a subnormal result and a subnormal input both come out zero. Inside each, arithmetic rounds to nearest, whatever rounding mode the host called in with; the shell counts the entries where the host arrived with another mode and shows the count with the output guard's. On return the host's control word is back bit for bit, also when the entry leaves by a throw. It is set per entry, not once per thread. | A build with the setter removed is red. A build with the restore removed is red. A stub host that enters with the mode already on, and one with it off, must each get its own word back. A stub host that enters rounding toward zero must get the pinned bits and its own rounding mode back. A build that constructs the engine outside the switch is red by a check at the shell's engine boundary, and NOT by the digests: measured 2026-10-11, the narrower scopes give the same bits on today's 543 rows, so the digests cannot see it. | full on macOS arm64; a Windows x64 leg, since the control word differs per architecture and the x86-64 half has never run on Windows | The tools' own controls: `control FZ` and `control ROUND` in `tools/h2_engine_stream.h`, printed by every gate that replays. | ADR-209 item 6; ADR-211 item 1; ADR-213; `docs/proposals/h2-flush-to-zero.md`; armor row 3; B430 item 3; catalogue R3 |

## 4. Build and platform (armor row 11)

ADR-209 item 8: 1.0 ships for macOS arm64 and Windows x64. Intel macOS is not a 1.0 platform, so
no universal build and no Rosetta leg are owed. B454 sends its items 2 to 7 to the shell's build
design, "where flags are set per target on purpose". Item 2 is row SC-1.

| # | Gap | Gate, and what it checks | Must-fail control | Runs | Legacy template, and what changes | Source |
|---|---|---|---|---|---|---|
| BP-1 | The legacy plugin target sets no optimisation level of its own. A bare configure builds it unoptimised. | `build_flags_check` gains a class for the shell's targets. Each carries its optimisation flags itself, per compiler. The built leg reads the emitted flags. | The flag removed from a shell target is a pin diff, red. A configure with no build type must still emit the pinned level, or the built leg is red. | fast (static); full (built) | `build_flags_check` and `tools/build_flags_pin.json` (108 targets, five classes). A new target with its class's exact flags stays append-allowed. A pin change is approved by the human; the mechanism is open (ADR-209 item 3). | B454 item 3; catalogue R11 |
| BP-2 | `CMAKE_OSX_ARCHITECTURES` is unset, so macOS builds whatever the machine is, under a comment that says Apple Silicon and Intel. | The pin records the shell's macOS architecture as arm64, set on purpose. The release job checks the shipped bundle's slices: arm64 on macOS, x64 on Windows. | The setting removed or widened is a pin diff, red. A bundle with a missing or extra slice is red. | fast; tag | `build_flags_check`, whose pin records the value as unset today. | B454 item 4; ADR-209 item 8 |
| BP-3 | CMake sets no warning flags. Only the Python-driven probes are warning-clean by rule. | The pin records the shell targets' warning flags, and the shell compiles clean under them. | A warning flag removed is a pin diff, red. If warnings are ruled to be errors, a planted warning in a shell file must fail the build. Which flags, and whether they are errors, is open question 2. | fast; full | `build_flags_check` for the pin. None for the clean build. | B454 item 5 |
| BP-4 | MSVC gets no floating-point or optimisation flags from this repo. | The pin records the shell targets' MSVC flags under the MSVC condition, and `build-windows` builds with them. | A flag removed is a pin diff, red. `/fp:fast` anywhere is already red. Which values are right is open question 2; the evidence is package P11's MSVC tolerance-parity leg. | fast; CI job | `build_flags_check`, whose fast-math list already holds `/fp:fast`. | B454 item 6; catalogue R11 |
| BP-5 | The C++ standard is gnu++20, with compiler extensions on. | The pin records the shell targets' extension setting, set on purpose. | The setting changed is a pin diff, red. If extensions are ruled off, a planted GNU extension in a shell file must fail to compile. | fast | `build_flags_check`. | B454 item 7 |
| BP-6 | Parity proves nothing about the plugin if the plugin is built with other flags. | Two rows. `h2_rules_check` rule 2 classifies the shell targets, so each carries `-ffp-contract=off`. The numerics flags of the shell targets equal those of the targets the parity legs build, per compiler. | `h2_engine_fma_control` must still fire. A shell target the rule does not classify is red. One numerics flag changed on either side alone is red. | fast | `h2_rules_check` rule 2 and `h2_engine_fma_control`. Today a flag changed on a target the rule does not classify passes it (`tools/build_flags_check.py` docstring). | Proposal §1 row R11 ("at shipped flags"); armor row 11; `h2/README.md` rule 7 |
| BP-7 | A public build must be signed and notarized. Today a tag with no Apple credentials ships ad-hoc bundles, "said plainly", and the built bundles do not verify as code-signed until `./install` signs them. | On a horde 2 release tag: Developer ID sign, notarize with status Accepted, staple, then `codesign --verify --deep --strict` on what ships, under horde 2's own bundle names and package id. The bundles are sealed as built. | Some credentials present and some absent is already red. A file changed inside a bundle after signing must fail the verify step. Whether a tag with no credentials is red is open question 3. | tag; full for the as-built seal | `sign-macos`, `tools/package_macos.sh`, `release_path_check`. New names and ids (ADR-186 §3). Windows signing is unruled (B412). | Armor row 11 and phasing 3; proposal §2 (signing parked until the shell); B456 (the as-built finding) |

## 5. Validators and hosts (armor row 8)

| # | Gap | Gate, and what it checks | Must-fail control | Runs | Legacy template, and what changes | Source |
|---|---|---|---|---|---|---|
| VH-1 | pluginval runs on the legacy VST3 only, and Windows skips its GUI tests. | `validate-windows` and `validate-macos` on the horde 2 bundles: pluginval at strictness 10 on VST3 and AU, GUI tests included. | A missing bundle fails the step. That is the wrong-name case that once validated nothing (`docs/ROBUSTNESS.md` §4). A build with a planted fault (state that does not round-trip) must fail pluginval. | CI job | Both jobs. They point at horde 2's bundle names. Windows GUI tests need the WebView2 runtime strategy (B432 item 1). AU through pluginval is new. | B427; catalogue R8 |
| VH-2 | Nothing validates the CLAP artefact. pluginval does not host CLAP. | clap-validator on the CLAP artefact, on macOS and Windows. | B427 demands a planted-fault control: a build with one planted conformance fault must fail. | CI job | None. The validator is a pinned download, so adding it is a human gate (proposal P18). Whether a pinned prebuilt binary exists for both platforms is unverified (proposal §7). | B427; armor row 8 |
| VH-3 | auval is a manual step, not a gate. | `auval -v aumu` on the installed, signed horde 2 AU must print "AU VALIDATION SUCCEEDED", for the build under test. | A triple that is not registered must fail, so a pass cannot come from the tool alone. A stale installed copy must be detected: the validated component's hash equals the build's. | hands or a dev-Mac receipt: it needs an install | The manual command (`docs/ROBUSTNESS.md` §4). horde 2's AU subtype is new, and its manufacturer code is still open (ADR-186 §3; `docs/H2-PLAN.md`). | B427; catalogue R8; B456 (auval needs an install) |
| VH-4 | State loading is not tested at every point in the plugin's life, or with a stream that returns short reads. | `h2_shell_state_stream_check`. The same state loads before activate, after activate and while processing, and restores the same values and the same render. The stub stream hands over one byte per call. | A loader that takes one read to be the whole state is red under the one-byte stream. A load that differs by timing is red. | full | `state_check` (save, fresh instance, load, bit-identical render). It gains the three timings and the short stream. | B427 ("State load before and after activate and while processing, with looped stream I/O") |
| VH-5 | No test that two instances share no static state. | `h2_shell_two_instance_check`. One plugin instance renders bit-identically whether or not a second runs beside it, on the same thread and on another. | A planted shared mutable static in the shell is red. | full; the thread-sanitizer job | None on legacy. Package P15 builds the engine-level half; this is the same test through the plugin factory. | Armor row 8; catalogue R8; proposal P15 |
| VH-6 | Offline render is named as a host edge case, and nothing compares it with realtime. | `h2_shell_offline_render_check`. The same patch and events rendered in realtime mode and in offline mode, at different block sizes, are bit-identical. | A planted branch on the render mode is red. | full | None. It leans on the engine's block-size bit-identity (proposal §1 row R4). | ADR-197 answers ("the two-instance and offline-render tests"); armor row 8 |
| VH-7 | The target hosts are untested. Reaper and Bitwig are UNVERIFIED, and the human owns none of the targets. | One recorded hands-on pass per target host and platform, on trial versions, before a public build. The checklist: load, play, automate, save and reopen, the preset click of HB-3, bypass, an offline bounce, two instances. | The release check is red when a target host has no recorded pass for this release line. | hands; tag for the existence check | The table in `docs/ROBUSTNESS.md` §4. No scripted matrix is built. Which hosts are on the list is open question 4. | ADR-197 answers (host matrix); B427; proposal §1 row R8 |
| VH-8 | Nothing in CI opens the editor on Windows. | The human's test in a Windows DAW before the first Windows release: the GUI loads, every control responds, PASTE works, a planted `location.href=` is cancelled, page-script clipboard reads are rejected while Ctrl+V still works, and a dragged file or link is refused. | The planted navigation, the planted clipboard read and the dropped file are the controls: each must be refused. | hands | The B447 list, written for the legacy GUI. The same rows run on horde 2's GUI, plus OG-3's count. The CI smoke test is held unless the human asks for it. | B447; ADR-205 (the external-drop row) |

## 6. State compatibility (armor row 7)

| # | Gap | Gate, and what it checks | Must-fail control | Runs | Legacy template, and what changes | Source |
|---|---|---|---|---|---|---|
| SC-1 | horde 2 has no parameter manifest, so its lock is a stub. The legacy enumerator `registry_dump` aborts and is never run. | `h2_param_lock_check`. Parameters are generated from the engine manifests, never hand-listed. The generated table is committed as a lockfile: id, key, min, max, default, taper, steps. Ids are declared, never assigned by order. Regenerating gives the committed bytes. The real plugin's enumeration equals the lockfile field for field. | A removed, renumbered, re-ranged or reused id is red. Shuffling the manifest's order must leave the lockfile byte-identical. A hand-edited lockfile is red. A plugin that enumerates one field differently is red. | fast (static); full (the real plugin) | `param_id_lock_check` with `param_id_dump` (19 controls). The source becomes the manifest, not `src/`. Key, default, taper and steps are added. The `h2` stub in `tools/param_id_lock.json` is replaced. | ADR-186 §5(a); B308 H4; B454 item 2; catalogue R7 |
| SC-2 | Defaults are deliberately not locked on legacy. A default change is caught only in review. | Part of SC-1: a changed default in the lockfile is red unless the same change carries an ADR reference and a migration entry (SC-6). Before the first release the entry may be empty, with its reason stated. | A planted default change with no migration entry is red. The same change with both is green. | fast | `param_id_lock_check`, whose docstring records why defaults were left out. ADR-201 shows the empty-migration case. | ADR-197 edit E; proposal P17 ("the lockfile with defaults"); catalogue R7 |
| SC-3 | After the first release, ids and normalized ranges are locked for good. Nothing compares the lockfile with the last release. | CI diffs the lockfile against the last release tag's. Every id is below `0xB00000`, where clap-wrapper's MIDI proxies start. | An id at or above the bound is red. A range changed against the tagged lockfile is red. With no release tag the check prints that nothing was compared, and the row stays a hole. | fast, with tags fetched | `param_id_lock_check`'s append-only rules, with a tag as the reference. | B428 item 1; ADR-186 §5(f) |
| SC-4 | Logic binds automation to AU parameter order. The VST3 class id is derived by clap-wrapper, which changed that derivation once. | `h2_identity_lock_check`. The AU parameter order equals a golden order manifest. The VST3 class id is pinned to an explicit value. | A reordered parameter is red. A class id that differs from the pin is red. | fast; full for the built bundles | `test_identity_check`'s `frozen` row, which pins the real identity against a table in the check and reads the built bundles in `full`. It pins no parameter order. | B428 items 2 and 3 |
| SC-5 | State must carry a schema version from day one, and no stored value may depend on position or field length. | `h2_state_schema_check`. Every saved state carries its schema version. State is keyed by stable parameter key: a state with its keys reordered loads identically. | A saver that omits the version is red. A loader that reads by position fails the reordered row. | full | `state_check`'s forward-compatibility rows (missing keys load defaults; unknown keys are ignored). | ADR-186 §5(b) and §5(g); B398 |
| SC-6 | Migrations must be data, with tests. Every default change owes one. | `h2_migration_check`. Each migration is a data entry: from version, to version, rule, ADR reference. For each one a stored pre-change state loads and renders its pre-change sound. After the stability line, a sound-law change is selected by the state header's version. | The migration removed, the pre-change state renders differently: red. A schema version bump with no entry is red. An entry with no stored state is red. | fast or full, by what the render needs | `tools/labharness/os_default_check.mjs`, the first ADR-197 migration (B445): five oracles, seven must-fail controls. | ADR-186 §5(b) and §5(f); ADR-197 edit E; proposal §1 row R7 |
| SC-7 | That a session restores its sound, not just its numbers, is proven for legacy only. | `h2_shell_state_check`. Every lockfile parameter is set off its default, saved, and loaded into a fresh instance. Every value reads back exactly, and the render is bit-identical. The parameter list is read from the lockfile and counted. | A parameter left out of the saver is red. A planted change to one loaded value must change the render, or the comparison is blind. | full | `state_check`, `statefix_check` (its calibration plant) and `modreadback_check`. All three are on the re-earn list (B308 H5). | Catalogue R7 ("legacy only"); B308 H5 |
| SC-8 | No per-release corpus of saved states. None may exist before the first release. | The mechanism only, built with the shell. A tool freezes saved states and their render digests at a release tag: digests and compact fixtures, never raw audio. `h2_state_corpus_check` loads every frozen state on the current build and compares its render. Before the first tag it reports a hole, never green. | Each run proves the mechanism on a corpus built in a temporary directory: one frozen state, then a planted default change, must read red. A fixture with no digest, or a digest with no fixture, is red. | full on the golden platform; CI or nightly elsewhere, by open question 5 | `statefix_check`, where an empty corpus is red and every fixture has a calibration plant. | Armor row 7; ADR-197 answers (the corpus starts with the first release); proposal §1 row R7 and §5; ADR-187 §7 |
| SC-9 | Before the stability line, projects saved earlier may change sound, "and the shell says so". | A pre-release build carries that notice where the user sees it. | **No control is known beyond presence.** A build with the notice removed is red only if the check reads the same string the user sees. | hands; fast for the string | None. | ADR-186 §5(f) |

## 7. Shell rows owned elsewhere, not restated here

These are shell acceptance too. Each has its own ROADMAP row and is not copied onto this page.

| Row | What it owns |
|---|---|
| B308 H3 | Position-free draws from a counter-based stream seeded by (seed, key). It needs its own ADR before the first horde 2 patch is saved. |
| B308 H5 | The 55 shell-linked legacy tools, re-earned as horde 2's acceptance list. This page names the ones the armor needs. The full list is not written (open question 10). |
| B429 | Tails and never sleeping while a tail rings, latency reporting, the wrapper boundary, one constant tail bound. |
| B430 items 4 and 6 | Click-free voice steals; cross-platform goldens. |
| B432 | The GUI on real platforms: WebView2 runtime, many instances, focus, DPI. |
| B449 | GUI rules R1 to R11: MIDI input never stalled by GUI activity; typed numeric entry. |
| B412, B433 | Licensing, installers, the update path, and the counsel items. |
| Packages P2 to P16 | The engine-level gates: fuzz, invariance, soak, closed forms, measurements, the engine's own realtime probe. |

## 8. Proposed additions, not sourced

No record demands these. Each is offered with one line of reasoning, for the human to take or drop.

1. **A static inventory of function-local statics in render-reachable code.** RT-2's probe proves
   only the statics its schedule reaches; a list with a "new static is red" rule covers the rest.
2. **Reactivation equals a fresh instance.** Armor row 8 names reactivation as a hiding place, but
   the only reactivation rows today are about realtime safety, not about what is rendered.
3. **A zero guard count at the end of every shell oracle that plants no fault.** A guard that
   repairs a NaN inside the test suite hides the root cause from the suite itself.
4. **A newer-than-known schema version is refused whole, with the reason shown.** A user who opens
   a newer project in an older build should get a clear refusal, not a half-loaded patch.
5. **The other counters share the guard's surface.** ADR-200 and ADR-207 count queue overflows,
   refused events and dropped MIDI bytes, but no record says who reads them; showing them beside
   the guard count costs little.
6. **Enrolment for the shell's entry points.** A CLAP extension the shell registers that no probe
   schedule drives would be red, in the way package P2 makes an un-enrolled engine red.
7. **The allocation counter on the Windows CI leg.** RealtimeSanitizer does not exist on Windows
   (B430 item 7), so RT-4 is the only realtime gate that could run on the second 1.0 platform.

## 9. Open questions for the human

1. **Where the sanitizer gates run.** Rows RT-1, RT-2 and RT-5 can be green only "in CI or a
   receipted nightly" (proposal §1). Whether this account is billed for macOS runner minutes is
   unknown, and night jobs on the dev Mac were not chosen (ADR-209 item 4 and "Open"). If no macOS
   nightly exists, do R1 and R2 stay partial, or is "dev Mac, `verify full` only" a dated accepted
   limit (ADR-209 item 2)?
2. **The flag values B454 leaves open.** Items 5 to 7 say what is missing, and B454 sends them to
   a design "where flags are set per target on purpose", but no record says which values: the
   warning set, and whether a warning is an error; the MSVC floating-point and optimisation
   flags; compiler extensions on or off. Each becomes a pinned value.
3. **A release tag with no signing credentials.** Today that ships ad-hoc bundles and says so.
   For horde 2's public builds, is it red? And is Windows signing in 1.0 (B412 is open)?
4. **The host list for hands-on trials.** The armor's answers name Bitwig, Reaper, Logic and FL
   Studio. B427 names REAPER, Bitwig, FL Studio and Studio One (partial). Which list, on which
   platforms, and is the human's own host on it?
5. **How the corpus is compared off the golden platform.** Bit-exact digests hold on one platform
   only (ADR-187 §7; proposal §5), yet armor row 7 says "loaded and rendered in CI". Bit-exact on
   the dev Mac with a receipt, a tolerance comparison in CI, or both? And what does a release
   freeze: the factory bank, every parameter off its default, or more?
6. **Does the output guard also report level?** B430 item 1 asks for a "non-finite and range"
   guard. ADR-209 item 7 rules the non-finite half only.
7. **The guard count in the GUI.** Where it is shown, and what clears it: a user action, or only
   a reset. Also, for the lead to confirm: is the latch's atomic counter an "audio-published
   snapshot" under ADR-200's shell rule 2, or must the count travel in the snapshot?
8. **B430 item 2's "deadline counter".** A wall-clock deadline cannot sit in `./verify`. Is RT-8's
   bit-identity row enough, with timing left to hands-on trials?
9. **The fuzz engine for HB-5.** B430 item 5 names libFuzzer. Its runtime is present in the
   Homebrew LLVM already used for sanitizers (ADR-199; checked on the dev Mac 2026-10-10). Whether the
   product compilers carry it was not checked. Is using it a new dependency, or should the fuzzer
   be a seeded mutator in the tree, which runs wherever the oracles run?
10. **B308 H5's list.** Should the 55 shell-linked tools be listed on this page before B398 is
    ratified, or stay a separate list?
