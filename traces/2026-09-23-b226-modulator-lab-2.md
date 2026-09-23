# b226-modulator-lab-2 — Kuro LFO, ORBITAL and MIDI gate/trigger/trackers join the modulator lab under round 1's one-law rule

- **Queue item:** B226 (row carried in PR #729, `lead-records-82`; dispatched 2026-09-23 by the horde lead). Round 1 is B208 (PR #722, trace `2026-09-22-b208-modulator-lab.md`).
- **Why:** The human asked for "some of the novel modulators in the modulator lab — the kuro-LFO and the orbitals system", and for "deeper MIDI-based trackers, including a note-on gate and a note-on trigger (for things like hitting the 'randomize unlocked' button on Sluice)". B226's acceptance has three parts. Each family runs under round 1's shape/preview rules. The trigger's event semantics are stated and demonstrated against at least one action target. Open questions are listed for the human.

## What was done

One file changed, `docs/design/shape-lab-mod.html`. Round 1 is untouched in behaviour. It gains a new section, **sources**, with six cards and a findings/questions card. `<meta name="lab-review">` now reads `B208 + B226 · 2026-09-23`. `docs/design/index.html` is not edited.

**One read path.** Every continuous source is a row in `SOURCES` with a `read(core)`: the round-1 LFO, Kuro voices 1-8 and R, ORBITAL bodies 1-4 × {x, y, speed, near, angle} plus field energy and spread, the seven MIDI trackers, and the two event→level converters (S&H, MOD ENV). A route reads a source there. The render logs history lanes there, and every lane is Float64, so a lane equals the value it logged. The painters read the same rows, or they draw those lanes. The TRIGGER is listed in the route picker as a **disabled** option, "an event, not a level". The refusal is pinned rather than absent (L0036).

**Kuro LFO (ADR-053).** `mod-lab.html`'s `KuroSwarm` is transcribed as a subset: n 1..8, bipolar K, rate/detune/anchor, the mean-field topology, and the rank-lattice splay (the A5 fix). The arithmetic is unchanged, including mod-lab's truncated `TAU = 6.283185307`. The one structural change is the splay's rank order. mod-lab allocates and sorts a fresh array every sample; the lab does an insertion sort into a preallocated `Int32Array`. The comparator is a strict total order, so both produce the same permutation. The ring and two-cluster topologies, the legacy splay law and the satellite `link` are dropped (reduce). A fifth shape, **DRAWN**, runs every voice through round 1's `shapeAt` on the LFO card's points, so every LFO in the lab has one shape law. mod-lab.html was read only.

**ORBITAL (ADR-165, SPEC-ORBITAL).** The reference's physics is page script, not an extractable class, so `accel()` (:307-323) and `step()` (:397-449) are transcribed. The edge cushion, both B126 regulators and pointer drag are left out, and all four are off in every reference preset (:226-227, :302). The spec's parts are added on top: stepping counted in **samples** (§6.2) at the lab's 1/480 s (ADR-165 amendment of 2026-09-17: the port's 1/960 is not a parity claim), the §4 observables, 5 ms smoothing primed on reset (§3.2, §6.3), and KICK and RESET (§7) as action targets. **Angle is not smoothed**, because a one-pole across the 1→0 wrap sweeps through 0.5. ADD BODY draws from this file's own mulberry32 stream, keyed by (seed, add count), per the brief. KICK's direction is a separate seeded stream. `reference/gravity-modulator.html` was read only.

**MIDI trackers.** `NoteTracker` is global, with a 16-key cap preallocated.
- **GATE** is legato: 1 while any key is held. It never dips on an overlapping note-on or on a non-last note-off.
- **TRIGGER** is an event: a count raised by the note-on and consumed at the top of the next rendered sample. EVERY fires on each note-on; FIRST fires only on the note-on that opens the gate.
- **NOTE** uses last-note priority with fallback to the newest held key, (n − 60)/48, and holds its value when no key is held. **VEL** follows the NOTE key. **REL VEL** is latched. **HELD** is count/8. **ORDER** is a round-robin k since the gate opened, wrapped at N. **SINCE** is seconds since the TRIGGER over T.

The on-screen key's height sets velocity and release velocity. PHRASE, a fixed 3 s phrase (`PHRASE`, `phraseEvents`), is scheduled sample-accurately through a core event queue, and the audit uses the same table.

**Event semantics and action targets.**
- S&H samples its input (seeded RANDOM or any continuous source) and holds it.
- MOD ENV restarts round 1's envelope law from the level it has reached. This needed one optional parameter on `envAt`: `v0`, which when omitted is identical to before. The gate falling releases it.
- LFO RESTART is round 1's retrig rewind, split out of `noteOn` unchanged as `lfoRewind()`.
- KURO RESET, ORBITAL KICK and ORBITAL RESET act on those modules.
- **HOSTED: RANDOMIZE UNLOCKED** posts (seq, fired-at sample) into a preallocated 64-entry ring. The main thread drains it into a **generic** stand-in module with six parameters, locks and its own seed. The stand-in draws only over unlocked parameters and pushes one patch JSON per undo step, with (seed, click) kept as provenance. That is the shape `integrations/sluice/notice-undo-history.md` describes. Sluice's internals are not modelled.

The EVENTS card draws the trigger as **ticks, not a line**, beside the gate (a level), the S&H (stepped) and MOD ENV (restarted curve).

## Evidence consulted

ROADMAP rows B226 and B208 (`origin/lead-records-82`); trace `2026-09-22-b208-modulator-lab.md`; `docs/design/shape-lab-mod.html` in full (at `7d08e6e`); `docs/design/mod-lab.html:75-248` (`KuroSwarm`); `docs/design/kuramoto-lfo-golden-spec.md` (including the 2026-07-24 amendments: A5 resolved, degenerate equilibrium, per-note vs global finding #2); DECISIONS ADR-053, ADR-165 and amendments 1/2/2026-09-17, ADR-166 and amendments 3/7; `specs/SPEC-ORBITAL.md`; `reference/gravity-modulator.html:130-739`; `integrations/sluice/notice-undo-history.md`; `tools/labharness/lab_load_check.mjs`; `tools/golden/extract_core.mjs`. LIBRARY lessons applied: L0020 (load fingerprint kept), L0026 (state above wiring; setup last), L0032/L0033 (controls that must fire; a plant that does not fire records a coverage boundary), L0036 (the pinned refusal), L0048/L0061 (own scratch folder and port), L0051/L0056 (verify on the committed hash, read `.harness/last-verify.json`).

## Verification (beyond ./verify)

All scratch work is under `scratchpad/b226/`. The scratch site is served on 127.0.0.1:8226 with symlinks to this worktree's `reference/` and `docs/design/`.

- **In-page round-2 audit** (headless Chrome, the page's own text), offline 8 kHz, 25,600 samples. Destination tap vs an independent twin:
  - KURO voice 1 max|Δ| **0** (control rate × 1.05: 0.475);
  - ORBITAL body 1 x **0** (control G × 1.1: 0.113);
  - MIDI, 7 trackers vs the rules restated independently, **0** (control, phrase 1 ms late: smallest 0.125);
  - picture lane vs destination tap **0** (control, 100 ms off: 0.577);
  - TRIGGER: **9** S&H steps at exactly the phrase's EVERY samples (FIRST would give 5, so the control fires); held values = the seeded draws, max|Δ| **0**.

  With FIRST as the default mode (scratch copy): 5 steps at exactly the FIRST samples, EVERY would give 9, all else 0.
- **Round 1's audit still reads 0:** LFO max|Δ| 0, ENV max|Δ| 0 (bit-identical), controls 0.100 / 0.100, mirror exact to 7.8e-16.
- **Kuro transcription vs `mod-lab.html`'s own `KuroSwarm`** (both sliced by the shipped `extract_core`, design banners). This covers 1,056 configs: n ∈ {1,2,4,6,8}, K ∈ {−1, −0.5, −0.2, 0, 0.2, 0.35, 0.7, 1}, detune ∈ {0, 0.3, 1}, anchor ∈ {0,1,2}, shape ∈ {0..3}. Twelve seconds at 44.1 kHz for the n = 4 mean-anchor set, 3 s for the rest. max|Δ| over lfo[], phase[], R and ψ is **0**.
  - Control (K + 0.05): 240 configs read under 1e-3. Every one at detune 0 with K ≤ 0 is the documented degenerate equilibrium, where coupling is ∝ R = 0. The other five are at K = 0, where the nudge adds 0.0075 Hz of coupling, or are the square shape, whose output does not move. That coverage boundary is why the **in-page** control nudges the rate instead.
- **ORBITAL transcription vs the reference's own `step()`.** The reference was loaded in headless Chrome, `paused = true`, `load(preset)`, then 4,800 × `step(DT)` inside the reference page. Four presets were compared (binary, sun, figure8, chaos), with x, y, vx and vy sampled every 48 steps. max|Δ| is **0** on all four. Control (body 1 starts +1e-9 in x): 3.96e-8 / 2.91e-6 / 2.76e-8 / 1.99e-5, so it fires. The reference's P for every preset shows cushK 0, thermoOn/vdpOn false and scale 1, as the transcription assumes.
- **Planted defects** (scratch copies, each anchor asserted to match once):
  - M1 route value × (1+1e-12): **RED**.
  - M2 gate dips on the retrigger sample: **RED**.
  - M3 trigger modes swapped: **RED**.
  - M4 S&H skips a draw: **RED**.
  - M5 lane logs the previous sample: **RED**.
  - M6 gravity ×1.01 inside the class both sides share: **GREEN**. This is expected, and it is the in-page audit's coverage boundary: a law change inside a shared class is invisible to a same-class twin. The reference-parity probe above is what sees it (L0033).
- **Live run** (headless Chrome, audio on, every action on, DRAWN, FOUR-BODY CHAOS, ADD BODY, KICK, FIRST mode, PHRASE ×2): **no page errors**. Blocks advanced, triggers fired, bodies went 4 → 5, and hosted posts were applied **23.2 ms** after the sample they fired on (one UI frame).
- **Load gate:** `lab_load_check` OK in 0.23 s (0.18 s before). `extract_core` (design banners) still returns `ModLabCore`, now with `KuroSwarm`, `OrbitalField`, `NoteTracker` and `SOURCES` (41 rows) as statics.
- **Leak:** the worktree has no `.leakcheck-names`, so verify SKIPS that leg. The main checkout's list was run against the file with the gate's own case-sensitive `-P` pattern: **no hits**. No machine paths.
- **Screenshots** (not committed): `b226-running-light.png`, `b226-running-dark.png` (full page, dark via `?theme=dark` as a load), and crops of the round-2 section, `b226-r2-light.png` and `b226-r2-dark.png`. Headless audio is throttled (0.4 s of sim in 10 s of wall time), so the screenshot wrapper suspends the audio and drives the page's own `core.render` in 512-sample blocks, draining the hosted ring after each block. The lanes are the real render's log. The hosted lag in those shots therefore reads < 1 block (0.6 ms), not the live 23 ms.
- **Verify:** `./verify fast` exit 0 on the working tree, and exit 0 on the committed lab change **ff67631** (`.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"ff67631"}`).

## Alternatives rejected

- **Loading mod-lab.html and the reference into the lab page** (iframe or fetch). file:// forbids it, and it would bind this lab to the other files' DOM and globals. The transcriptions were chosen instead, with scratch bit-parity as the evidence. Whether to gate them with a committed `tools/*_check` is the lead's call, since it means a `./verify` edit.
- **A trigger routable to a level, with depth scaling a 1-sample pulse.** An event has no value between ticks, so depth scales nothing. The refusal is shown instead, and conversion goes only through stateful actions.
- **A gate that re-attacks (dips) on an overlapping note.** That puts the trigger's job in a level. It was planted as M2 and the audit catches it.
- **Modelling Sluice.** The brief forbids it. The stand-in is generic, and only the boundary (message, main thread, undo shape) is designed.
- **`accel` with a guessed ceiling.** ADR-165 A1 rules a fixed per-preset ceiling but gives no value. It is left unbuilt and listed.

## Open questions

These are listed on the page, card "ROUND 2 · FINDINGS AND OPEN QUESTIONS".
1. May a trigger fire a hosted module's action across the module boundary? If yes: latency (main thread, not sample-accurate), undo flooding (coalesce or rate-limit?), and whether the resulting parameter changes are written to host automation.
2. Trigger default, EVERY or FIRST? The shipped LFO retrig is this trigger → LFO RESTART under EVERY, so one mode control could replace per-module retrig switches.
3. Release trigger, and sustain pedal (CC64) holding the gate: not built.
4. Per-voice twins of velocity, note and time since note-on (the per-note/global distinction from mod-lab finding #2).
5. Tracker ranges: NOTE's (n − 60)/48 and last-note priority, ORDER wrapping, SINCE resetting with the trigger.
6. ORBITAL's speed/energy normalisation reads about 0.1/0.01 on shipped presets. `accel` is unbuilt (no ceiling value). Angle is unsmoothed.
7. ORBITAL's source count vs fixed matrix slots (B126, still open); the lab exposes bodies 1-4.
8. Neither transcription has a committed gate. That needs a new `tools/*_check` wired into `./verify`, which is the lead's call.
9. **Stale charter line:** CLAUDE.md §Domain says ORBITAL's add-body seed is "outstanding". `reference/gravity-modulator.html:726-731` and ADR-165 amendment 2 show it landed on 2026-09-15. The brief repeated the stale claim. The lab seeds its own add-body draw regardless.
