# b370-envelope-hierarchy-lab — the envelope hierarchy lab: ENV 1 global, law A/B by ear, onset scatter four ways

- **Queue item:** B370, read verbatim from `origin/lead-records-146:ROADMAP.md` beside B335 and B366.
- **The human, verbatim:** "It might be worth workshopping the full envelope hierarchy in a lab (this
  would be a complement to the mod lab). I think want to follow the Serum approach of having ENV 1 be a
  global envelope that applies to all pre-FX sound-generating devices (including filters). Then we need
  to determine the envelopes work for things like onset scatter (an important knob I would like to
  preserve; but I'm frustrated by the trade-off of eating up an envelope slot with it versus giving it a
  secret proprietary envelope which may clash with certain settings). … Maybe there's a creative
  solution I'm not considering."
- **Why:** B366's conflict inventory (PR #863) left 17 decisions, decision 1 (whose law) gating most of
  them, and the human asked for a lab to decide the hierarchy and onset scatter by eye and ear.

## What was built

- `docs/design/envelope-hierarchy-lab.html` (new, single file, gui2's tokens, `lab-review` meta).
  - **Sound:** B298's composed engine, loaded exactly as the SCALPEL lab loads it (two `<script src>`
    and one fetch of `reference/swarmsaw.html`). Not edited, nothing copied.
  - **How a prototype is heard (section A2).** The engine renders STEMS with its own envelope
    neutralised: A 0 and S 1 make `v.env` exactly 1; the output stage is made linear and inverted by
    `atanh`; smoothed controls are snapped. The lab's harness multiplies its envelopes into the stems
    and applies the engine's `tanh` itself.
    - Member stems: a harness subclass rewrites the pan gains after each member's step, so each pass
      carries two members. Measured: they sum to the full voice within 1.3e-7.
    - Source stems: base, + blade 1, + blade 2. A blade's stem is what switching it on adds.
    - Option 3 ("today") is the engine itself: its own envelope and B335's hidden mechanism.
  - **§1 the hierarchy.** A flow diagram drawn from the state. ENV 1 is applied per source at the mix
    (the same as after a linear filter when every button is on). ENV 2–4 go to the cutoff (a stand-in
    filter), blade 1's cut rate (sent to the engine per 128-sample block) or the sub (a stand-in
    sine). Every source has an ENV 1 button. There is a lifetime plot, and a rule switch: the
    inventory's "a shaping envelope" (the default) or Serum's "any envelope". The note under the plot
    states what "the voice lives until the longest release" does to freeing, B310's tiers and B323.
  - **§2 decision 1 by ear.** B366's twelve envelope presets (the same patches and envelopes, in
    seconds), each played as a short phrase. There are ▶ A, ▶ B, ▶ A then B, and ▶ blind (a
    seeded pick, with reveal). The two laws are drawn to scale, with the free times and the times to
    0.995. A and B share their stems, so the second listen is instant.
  - **§3 onset scatter, four ways:**
    - (1) the spread;
    - (2) a scatter slot (ENV 4 spent);
    - (3) today (the engine itself, voiceEnv or entry ramp);
    - (4) the lab's find, THE HEALING SPREAD: δᵢ close up by the time ENV 1 reaches its sustain.
    - Each is drawn with its band, members, ensemble mean and lead-in. Four clash views, each audible:
      a filter following ENV 1 (mean normalised, earliest, or per member); release and freeing;
      mono/legato with Legato Inverted and a must-fail pure-time reading; steals and B323's cap.
    - A measured evidence table and a verdict.
  - **§4** AHDSR hold, drawn over hold 0; a legato phrase with per-envelope Legato Inverted and the
    retrigger start (level or zero).
  - **§5** decisions 2–17. Each is a toggle or a drawing where it can be shown or heard. The
    inventory's recommendation is the default, marked ★. D4, D10, D12 and D13 have cards (D13
    measured on a real stem).
  - **§6** the self-check. `?smoke=1` renders every ▶ once and writes the result into the page.
- `tools/labharness/lab_wheel_scroll_check.mjs`: the lab joins LABS (`state: 'S'`). It adjusts on
  alt + wheel through the byte-identical `onAltWheel`, which the check's static layer requires to be
  inventoried. This is a wired addition (ADR-180 §1); nothing was relaxed.

## Measured (headless Chrome, served by `tools/serve_labs.py`)

- **Self-check 26/26, 13 must-fail controls:**
  - **SR:** stage times identical at 44.1 and 48 kHz (worst 15 µs).
    - CONTROL: coefficients frozen at 48 kHz, off by 53 and 223 ms.
  - **HOLD0:** SCALPEL's AHDSR at hold 0 is bit-identical to the ENGINE's own `v.env` (0 of 57441
    samples differ). Horde's is bit-identical to ADR-021's expressions (0 of 151399 and 152554).
    - CONTROL: a one-sample hold slip.
  - **OPTOUT:** an opted-out sub stays exactly 1 through ENV 1's release. The voice frees at ENV 4's
    release end + 8 ms (Δ −1 sample).
    - CONTROLS: button on; ENV 4 routed nowhere.
  - **CULL:** as a voice gain, the cull silences an opted-out source in 385 samples.
    - CONTROL: written into ENV 1, the source still sounds at 1.000.
  - **DRAWS:** the lab's δᵢ are the engine's own (4 notes × 7 members bit-equal). Lag-1 over 300
    notes: 0.974 / 0.682 / 0.016 at α 0 / 0.25 / 1 (ADR-077: +0.985, +0.679, −0.072).
    - CONTROLS: a memoryless stream, and i.i.d. draws (lag-1 −0.014).
  - **LEGINV:** ENV 2 inverted rises 0.751 in 15 ms at a legato note, and ENV 3 does not.
    - CONTROL: not inverted.
  - **STEM:** the harness's SCALPEL law on a stem is within 0.243 % RMS of the engine's own envelope.
    - CONTROL: horde's law, 85.4 %.
  - **SPREAD:** at scatter 0 every member IS ENV 1 (0 samples differ). HEAL: members within 1.45e-6
    of ENV 1 after the heal.
    - CONTROLS: scatter 5 ms; unhealed 0.012.
- **Smoke:** 47/47 play buttons render finite, non-silent audio (12 presets × 2 laws, §1, §4, §3's 5
  options and 16 clash scenarios). The first run found the mono records silent (their key-ons were
  recorded under the wrong field), which was fixed before this commit.
- **Evidence, pluck, scatter 20 ms, seed 1234, 7 members** (spread · slot · today · heal):
  - onset 90 % at about 47 ms for all four;
  - the latest member's own peak: 1.00 · 0.27 · 1.00 · 1.00;
  - the raw mean's peak: 0.72 · 0.40 · 0.72 · 0.71;
  - tail after the key-off: ×1.56 · ×1.00 · ×1.56 · ×1.00;
  - members apart at the key-off: 72 % · 0 % · 72 % · 0 %;
  - a mono retrigger under a pure-time read: 0 % for all four, where the state machine does not dip;
  - the steal step, today against D6: 0.016 against 0.006 (spread).

## Evidence consulted

- ROADMAP B335, B366 and B370 (the Serum 2 summary is the lead's; no manual text is copied).
- `docs/design/scalpel-envelope-conflicts.md` and the three b366 traces, on `scalpel-master-env`.
- B366's `ENV_PRESETS` in `docs/design/scalpel-interface-lab.html` on that branch.
- `docs/design/mod-matrix-lab.html` (the mod lab; `mod-lab.html` is superseded by it).
- `docs/design/scalpel-horde-engine.js` (armMembers, memberStep, stepM, the cull, noteOn).
- `reference/scalpel/prototype/razor-core.js` (render, startVoice, monoOn), read only.
- `src/swarm_core.h:640-690`, `:946-948`, `:1190-1232`; `src/force_core.h:61`.
- DECISIONS ADR-021, ADR-077, ADR-078, ADR-083.
- `tools/labharness/lab_load_check.mjs` and `lab_wheel_scroll_check.mjs`; `tools/gen_lab_index.py`.

## Alternatives rejected

- **Editing the engine to add HOLD, the horde law or the spread.** Out of scope: the brief says to
  prototype in the lab.
- **Rendering every audition natively.** It cannot play a law or structure the engine does not have.
  The stem method is measured (STEM) instead of assumed.
- **A per-member × per-source stem split.** It costs 3 × ⌈N/2⌉ passes. §1 plays with scatter off and
  §3 with every button on, and the page says so.
- **Recommending option 1 or option 4 alone.** They are one mechanism at two heal settings, so the
  recommendation is the pair.
- **Regenerating `docs/design/index.html`.** Left to the lead, as for recent lab PRs.

## Verify

- **9cc5ddf (the lab and the wheel inventory):** `./verify fast` exit 0,
  `{"target":"fast","exit":0,"git":"9cc5ddf","ts":"2026-09-29T22:05:00Z"}`.
  - It ran on the committed hash, not chained to the commit, with no Chrome job running beside it.
  - `lab_wheel_scroll_check: GREEN — 0 failure(s); 7 labs, 2 planted faults`. The new lab's rows:
    "plain wheel over 33 element(s): 0 value change(s), 0 preventDefault(s); alt wheel moved 31/33"
    and "alt wheel adjusts at 1/1 onAltWheel call site(s)".
- **This trace's commit:** re-run on its own hash. The result is in the PR and the handback, because
  a trace cannot carry its own commit's result.

## Open questions

- Decision 1 is the human's, by ear, in §2.
- The scatter recommendation (spread + heal, default auto) is the implementer's, for the human to rule.
- Should sᵢ scale the decay (the inventory's open question)? It is a toggle in §3, default A and R.
- The sub is a stand-in sine and the filter a stand-in SVF. Their laws are not proposals.
- The ensemble-mean normaliser takes its peak from the whole note here; a voice would compute it at
  note-on from the draws. That is unbuilt.
- `docs/design/index.html` is not regenerated.
