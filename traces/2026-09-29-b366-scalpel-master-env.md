# b366-scalpel-master-env: the Specimen settles; a master envelope on MAIN and OSC; compact macros; envelope presets; the conflict inventory; a hue-gradient logo

- **Queue items:** B365, B366 and B368. B365 and B366 were read verbatim from
  `origin/lead-records-144:ROADMAP.md`, beside B306, B310, B323, B325, B335 and B355. B368 was
  dispatched mid-task by the lead ("one more item for your branch, as a SEPARATE commit at the end";
  the lead records it as ROADMAP B368).
- **The human, verbatim:**
  - "the specimen always keeps a ripple going after all notes have ended."
  - "The macros on the MAIN take up an unnecessary amount of space, and it would be worth hearing
    sounds with different envelopes; could you please add a master envelope and put it on both main
    and the osc page for Scalpel? It would also be worth generating some new presets to test it. -
    This may surface some conflicts with the way envelopes currently work and the way they need to
    before it's ready for prime time"
  - "Is the logo gradient a hue gradient or a value gradient? It's hard to tell. I would be curious to
    also audition a hue gradient that's a bit more dramatic, and also at more of a diagonal."
- **Why:**
  - **B365.** The Specimen must be driven by what was rendered (L0064) and come to rest in silence.
  - **B366.** The master envelope is the voice's existing ADSR, exposed rather than reinvented (see
    below). The conflict inventory is the human's explicit ask.
  - **B368.** The human asked for an audition, beside the existing styles.
- **Files:**
  - `docs/design/scalpel-interface-lab.html`;
  - `docs/design/scalpel-envelope-conflicts.md` (new);
  - this trace.
  - Not touched:
    - the engine (`docs/design/scalpel-horde-engine.js`), `composed_engine_check.mjs` and the
      divergence ledger. The master envelope needed no new engine behaviour.
    - `tools/patchspace/dependency_tree.json`: it did not drift (`dependency_tree_check` GREEN,
      113 params).
    - `reference/**`: not touched.
- **Commits:**
  - a24c382 (B365);
  - 590eb41 (B366);
  - c3a7bae (B368);
  - this trace on top.

## Evidence consulted

- **ADRs:**
  - ADR-083;
  - ADR-187, with its A1;
  - ADR-189, with its A1;
  - ADR-021, ADR-084 and ADR-009 (via the charter).
- **Ledger:** `docs/port/divergences.json`, with `tools/labharness/divergence_ledger_check.mjs`.
- **The oracle, read only:** `reference/scalpel/prototype/razor-core.js`. The envelope is at
  :740-742 and :777-779; startVoice/noteOff at :394-434; the gain at :862; the parameter smoother
  at :746; polyMode releases at :357.
- **The composed engine:** `docs/design/scalpel-horde-engine.js`.
  - The header, :174 ("NOT COMPOSED THIS ROUND: ... ADSR").
  - B310's noteOn and tierPick, :501-530.
  - B323's cull, :533-550 and :1040-1052.
  - B325's look-ahead, :580-599.
  - B335's armMembers and memberStep, :643-733.
- **References and legacy:**
  - `reference/swarmsaw.html`: Kenv at :355, dissolve at :387.
  - `src/swarm_core.h`: ADSR at :160-165 and :1194-1228; cull at :1000-1024; tiers at :1669-1690;
    initVoice at :622-641.
- **Accounting and presets:**
  - `docs/scalpel/ACCOUNTING.md` rows 17-20 (:148-151, :396): A, D, S and R MERGE, and horde's law
    wins.
  - `reference/scalpel/data/presets.json` and `parameters.json`.
- **Prior traces:** b323, b334, b355-rework and b364.

## B365: the ripple

- **Measured first** (headless Chrome, real WebGL, audio on). A3 held 1 s and released. 4 s after
  the key-off:
  - the **monitor** still held note 57 gated (`monGated 1`);
  - the Specimen's front bound to 57 read held 1.00, the shader's travelling held ripple at full
    amplitude;
  - the quiver read 0.35-0.45;
  - `SPECIMEN.time` kept advancing.
- **Two causes.**
  1. **A stale feed.** The pearl read `MON.core` and `MON.L`. The monitor holds its note by design
     and never hears a key-off (L0064).
  2. **A time-driven animation.** The metaballs orbit on `uTime`, and the turntable turns on dt,
     whatever sounds.
- **The fix.**
  - **The feed.** The worklet shell now posts a rendered feed every 16 blocks (2048 samples,
    43 ms): the peak of the rendered samples and the engine's voices (`feedOf`, `blockPeak`). The
    ScriptProcessor fallback posts the same.
  - **Which source the pearl reads.** `specimenSource()` hands the pearl the audio's feed while the
    audio runs. With the audio off it hands over the monitor's, which is unchanged.
  - **The physics step.** It is now `specimenStep`, which uses no GL, and `specimenFrame` uploads
    its uniforms.
  - **The activity level, `act`.** It is 1 while sound plays, meaning a voice is alive or a window's
    peak is at or over −90 dBFS. In silence it decays with τ 0.15 s and snaps to exactly 0 under
    1e-3 (1.04 s).
  - **What `act` scales.** Time and the turntable are scaled by it, and so are the ripple and quiver
    amplitudes. At rest nothing is redrawn.
  - While sound plays, act is exactly 1 and the step is the pre-fix arithmetic, bit for bit.
- **Live after the fix.** A3, same run: 1.0 s after the key-off act was 0.148; at 1.5 s, 0.006; at
  2.0 s, 0. From then draws stopped at 347, and `specimenEnergy` read 0.
- **checkB365 (in-page):**
  - A3 held 0.3 s; every voice freed at 0.98 s; the output under −90 dBFS from 0.98 s to the end.
  - Motion energy: up to 2.211 while sounding; 1.12e-3 at 1 s into the silence; exactly 0 from
    2.05 s (1.07 s into the silence) through the 35 windows after +1.1 s.
  - 23 of 23 sounding windows are bit-identical to the pre-B365 law.
  - **CONTROL:** pre-B365 fed by a monitor reads 2.180 at 3.5 s (the held ripple at 1.00).
  - **CONTROL:** the pre-B365 law on the right feed reads 1.002 (the orbit).
  - The load-meter row now splits the shell's two kinds of posts. It requires exactly the load
    windows (2) and the Specimen feeds (4), each well-formed, and nothing else.

## B366: the master envelope, the macros, the presets, the conflicts

- **Investigated before building.** The oracle's voice already runs a proper ADSR on A, D, S and R:
  - a linear attack;
  - exp(−4t/D) toward S;
  - exp(−4t/R);
  - the voice freed under 1e-4;
  - every coefficient from seconds × the rate on each render call (ADR-009).

  The composed engine leaves it as it is. **So no divergence was needed, and none was built.** A
  flag would only have re-implemented the oracle. The brief's divergence path (D5, all-off rows,
  composed_engine_check rows) therefore did not apply. The law's contract is gated in-page instead
  (below).
- **Built:**
  - **Compact macros.** MAIN's eight macros are one row of the face's standard knob (`.mgrid.row`),
    each label one line. The cell went from 148.2 to 79.9 px.
  - **One control set, two views.** `envPanel` draws A/D/S/R on MAIN (its own "Envelope" cell,
    under the macros) and on OSC (the strip's second line). Both go through the one `knob()`. A, D,
    S and R left One level down (`tier2`, `T2_KEYS`). Times show in seconds (SP unit `mss`); the
    stored value stays the oracle's ms key.
  - **The curve.** It is the law at the knob values, each stage's width proportional to √time, and
    it shows the tail (key-off to freed).
  - **The stage indicator.** It follows the newest voice in the rendered feed (`feedOf` gained
    `lst`/`lenv`), or the monitor while the audio is off.
  - **Twelve presets.** `ENV_PRESETS` in the category "Envelopes (lab, B366)", appended after the
    bench's 83. They use SCALPEL keys only, so checkPresets holds them against parameters.json
    (95/95 clean, 95/95 play). They are marked `lab`, so they never read as "bench voicing".
- **checkB366 (in-page; every row with a must-fail control):**
  - **One state.** A real pointer drag on MAIN's Attack moved it 0.0040 → 0.027 s. The engine reads
    0.027 s, and OSC's Attack reads "0.027 s". Each page holds exactly one of each of A/D/S/R, and
    One level down holds 0. **CONTROL:** a planted view with its own copy reads "0.0040 s".
  - **The law at 44.1 and 48 kHz.** Attack 100.00 ms; at D 0.5092 (law 0.5092); sustain 0.5000; at
    R 0.0183 (e^−4); tier-1 "faded" at 435.0 ms; freed at 596.2 ms; then −113 dBFS. Identical at
    both rates. **CONTROL:** timed as 48 kHz samples, the attack reads 91.88 ms.
  - **The indicator.** On a scratch engine's own feed it read A → D → S → R → idle. **CONTROL:** on
    the monitor's feed it never reads release.
  - **The macros.** 79.9 px (≤ 60% of 148.2); 8 knobs in 1 row, all inside and legible. Osc
    Controls end inside the frame. **CONTROL:** the pre-B366 grid measures 148.2 px.
  - **The presets.** Each plays its named envelope, read as the gain the engine applied, Σ env·vel.
    The audio's 10 ms RMS was tried first and rejected, measured: the swarm's beating moved Gated
    stab 17.4 dB inside a held note. The pile-up keeps its held C3 and loses the two quietest tails.
    **CONTROL:** the oracle's default envelope fails pluck, swell and tail.
- **The conflict inventory** is `docs/design/scalpel-envelope-conflicts.md`. It has 16 facts
  (F1-F16, marked gated or measured), 10 mechanisms, the gain chain, who frees a voice, and 13
  decisions with recommendations. The one-off measurements were scratch Node renders of the
  composed engine, 48 kHz, seeded; each row states its method. The key ones:
  - **F3:** SCALPEL's law against horde's with the same numbers: release 4× apart, attack about 5×
    apart.
  - **F4:** a held S 0 note keeps its voice.
  - **F5:** the cull ends an 11 s tail in 10.7 ms.
  - **F6:** the at-cap replacement has no fade (0 clicks).
  - **F8:** a stolen slot starts at 0.646.
  - **F9:** blade env 0.656 is left at the free.
  - **F10:** a mono retrigger drops the cut rate 2.28× → 1.05× (6.4 dB worst frame, not a click).
  - **F12:** onset scatter puts 90% of the peak at 52 ms.
  - **F14:** 10.5% of the onset burst is left under a swell.
  - **F16:** the tanh squeezes a −6.02 dB sustain to −4.54 dB at gain 1.

## B368: the logo's hue gradient

- **The answer for the record:** GRADIENT is a VALUE gradient.
- **Two styles** end the cycle (the seven existing entries are unchanged):
  - `hue-diag` "HUE ↘";
  - `drop-inset-hue` "DROP + INSET + HUE ↘".
- **The gradient.**
  - It runs 150° of OKLCH hue (fill hue ±75°) along a 35° axis, stretched so the box's top-left and
    bottom-right corners are its two ends, with 13 stops.
  - It holds one OKLCH lightness (the fill's, capped at 0.80) and the fill's chroma, lowered only
    where sRGB cannot hold it (bisection on C).
- **Measured, the reason for the cap.** At its own L (0.93) the drift's pale yellow fill left the
  cyan end at C 0.033, a grey, and its hue turn read 146.4° of 150.
- **checkB334, extended** (2 themes × 6 fills round the drift):
  - The least hue turn is 148.4° (gate ≥ 148: a corner pixel's centre is half a pixel inside, about
    0.8° at each end).
  - Lightness spread ≤ 0.006 (gate 0.03).
  - Chroma is never grey (least C 0.074, on a saturated blue fill whose own C is 0.257: sRGB
    physics; gate ≥ 0.06).
  - **CONTROL:** the value gradient turns ≤ 7.2° and spreads L ≥ 0.112.
  - **Siblings:** each new style differs from its value-gradient sibling by 3740 and 3732 px (≥ 200).
    **CONTROL:** a HUE ↘ planted with the value gradient is 0 px from GRADIENT.
- **Cost** (the generic row, warp included):
  - FLAT 2.30 ms; GRADIENT 2.30; DROP + INSET + GRADIENT 2.31;
  - HUE ↘ 2.39; DROP + INSET + HUE ↘ 2.40 (now the heaviest).
  - Shedding with it: 30 → 8 paints in 60 frames.

## Self-check totals

The in-page self-check reads **119/119 (51 controls)** at c3a7bae (headless Chrome with WebGL).
The rows added here: B365 4, B366 10, B368 4. Before the branch the self-check had 101 rows, which
is entailed from the counts, not re-run on origin/main.

## Alternatives rejected

- **B365:**
  - Keep the monitor as the feed and add a note-off to it. Rejected: the monitor's held note is
    what the other views draw, so the pearl would still not be driven by what was heard.
  - Freeze the pearl the moment the voices end. Rejected: it would stop mid-ripple; the settle is a
    fade.
- **B366:**
  - A new ADSR as a D5 divergence. Rejected: the oracle already has one (reduce, never invent).
  - Measuring the preset shapes from audio RMS. Rejected: the swarm's beating hides the envelope
    (17.4 dB).
  - A fixed 80 px bound on the macros. Replaced by 60% of the old grid, because 79.9 px sat on the
    edge.
- **B368:**
  - Hold the fill's own lightness. Rejected: pale fills go grey, measured.
  - Hold constant chroma at the span's minimum. Rejected: a saturated blue fill would drop to 0.074
    everywhere, which is not "more dramatic".
  - HSL hue rotation. Rejected: not perceptual, and lightness would swing.

## Verify

Each run was on the committed hash, never chained to the commit. The records are
`.harness/last-verify.json`.

- **a24c382 (B365):** `./verify fast` exit 0.
- **590eb41 (B366):** `./verify fast` exit 0.
- **c3a7bae (B368, the code complete):**
  - `./verify fast` exit 0: `{"target":"fast","exit":0,"git":"c3a7bae","ts":"2026-09-29T20:43:42Z"}`.
  - `./verify full` **exit 1**: `{"target":"full","exit":1,"git":"c3a7bae","ts":"2026-09-29T20:45:28Z"}`.
    Verbatim:

    ```
    verify[full]: STATION lab check FAILED
    FAIL  S20  cost: reported, with the linear-voice-scaling gate   [1.9s]
      REPORT (this machine, Node v24.10.0): 1v 2.30 %  8v 17.17 %  16v 40.18 % of realtime
      linearity  8v/8x1v 0.93   16v/16x1v 1.09   agreement 0.158
      (gate: both in [0.4, 1.6] AND within 0.15 of each other ...)
    RED — 21 checks, 1 failed
    ```

  - This gate was not touched here (STATION's lab, `tools/labharness/station_check.mjs`). It is a
    wall-clock linearity gate. It failed while I was running the lab's in-page self-check in
    headless Chrome at the same time: my own load (the B362 pattern).
  - Re-run standalone, with nothing else running: `node tools/labharness/station_check.mjs` exit 0.
    S20 PASS: "linearity 8v/8x1v 0.88 16v/16x1v 0.91 agreement 0.030", "GREEN — 21 checks, 0 failed".
    Nothing was relaxed.
- **This trace's commit:** `./verify fast` and `./verify full` were re-run on it with nothing else
  running. Their verbatim result is in the PR and in the handback to the lead, because a trace cannot
  carry its own commit's result.

## Open questions

- **B365.** The ring, the cycle view and the carpet still draw the monitor's held note after a
  release. Should they go quiet too? With the audio off, the pearl still follows the monitor, as
  every view does.
- **B366.** The conflict inventory's decision 1 (whose envelope law: SCALPEL's or horde's, as
  ACCOUNTING rows 17-20 ruled) gates most of the rest. Until it is ruled, the lab's envelope times do
  not predict horde 2's.
- **B366.** A, D, S and R are placed at T1 on the face by the human's ask. The tier table reports
  them as promoted.
- **B366.** The lab's keyboard sends velocity 0.85 always, so velocity (decision 3) cannot be
  auditioned here.
- **B368.** Whether 150° and 35° are right. The dark-theme DROP shadow is unchanged, so it still
  reads as a violet halo there.
