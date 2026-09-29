# b365-idle-correction — act gates only the ripple/quiver; idle motion runs unconditionally

- **Queue item:** correction to ROADMAP B365 (dispatched as a standalone brief, horde lead session
  2026-09-29; not a new B-number — see `origin/lead-records-147:ROADMAP.md`'s B365 row).
- **The human, verbatim** (after B365 as merged in PR #863): "I still want the specimen to shift
  around when notes aren't playing, I just don't want the note-dependent ripples to trigger unless
  there's a note pressed."
- **Why:** B365 as merged had the settle variable `act` gate every time-driven term in
  `specimenStep` — the metaball drift (`S.time`), the turntable (`S.yawT`), the coherence smoothing
  (`S.kSm`), AND the note-driven ripple/quiver (`rAmp`, `shy`) — via one `dtA = dt * act`. That froze
  the whole pearl at rest, which the human did not ask for: only the ripple/quiver should stop when
  nothing sounds.
- **Files:**
  - `docs/design/scalpel-interface-lab.html` (the only file in scope; nothing else touched).

## What changed

- **`specimenStep`** (:4057-4098): the single `dtA` is now two: `dtI` (idle — real `dt`,
  unconditionally) drives `S.yawT`, `S.time` and `S.kSm`'s smoothing; `act` now scales ONLY the
  returned `rAmp` and `shy` (the strike/held ripple and the quiver), exactly as B365 built them. A
  new `S.freezeIdle` flag (default `false`, never set on the live `SPECIMEN`) reproduces the
  merged-B365 bug for the check's control by making `dtI = dt * act` too, without a second copy of
  the function.
- **`specimenFrame`** (:4114-4139): dropped the B365 "at rest, nothing is drawn" skip
  (`S.act === 0 && !specSounding(src) && ...`). It no longer applies — idle motion never reaches
  rest — so the frame falls through to gui2's original shed-only throttle
  (`(++S.n % (LOAD.shed ? SHED_SPECIMEN : 3))`), unchanged from pre-B365.
- **`specimenEnergy`** (:4101-4108): dropped `U.act` from the sum. It used to measure "is the idle
  orbit still running", which must NOT read 0 anymore; the function is now the RIPPLE/QUIVER energy
  only (front strike + held, scaled by `rAmp`; `shy`; the dent, which was already `dt`-driven and
  self-damping, unaffected by this correction).
- **`checkB365`** (:7157-7221): kept the ripple/quiver-settles-to-0 row (now reading
  `specimenEnergy` without the act term), the bit-for-bit-while-sounding row, and the monitor
  stale-feed control (all still valid, unaffected by the correction). Replaced the row that no
  longer holds — **"CONTROL: the pre-B365 law on the audio's feed still moves (the time-driven
  orbit)"** — because post-correction the FIXED law also never stops the orbit, so it stopped being a
  distinguishing control (see "row whose expectation changed" below). In its place: a new pair
  measuring the IDLE motion directly — summed `|Δtime| + |Δyaw|` over `[tQ, tQ+2.0]` s of silence,
  required `> 1.0` for the fix, and `< 0.3` for the `S.freezeIdle` control (the merged-B365 bug).
- **`b365Shots()`** (query-param gated, `?b365shots=1`, new, near `bootLate`): drives the live
  Specimen through idle / held / released moments by setting `AUD.on`/`AUD.feed` directly (no real
  `AudioContext` needed — same feed shape `feedOf` already hands the shader) and hand-advancing
  `MON.clock`, capturing each moment with the canvas' own `toDataURL()` into a hidden `#b365shots`
  div so a headless `--dump-dom` can pull the PNGs out as base64. Same idiom as the existing
  `misreadAll`/`probeAudio` query-param harnesses; changes nothing when the param is absent.

## Row whose expectation changed (as the brief asked to call out)

- **"CONTROL: the pre-B365 law on the audio's feed still moves (the time-driven orbit)."** Before
  the correction this proved cause #2 (the orbit was gated by `act` in the FIX, but not in `legacy`).
  After the correction the FIX no longer gates the orbit either — idle motion is unconditional in
  both — so the row no longer distinguishes anything. Replaced by the idle-motion row + its
  `freezeIdle` control, which test the actual thing now at stake: does idle motion survive silence
  (fix: yes) or freeze (the merged-B365 bug: yes, and that's the must-fail control).

## Measured (headless Chrome, real WebGL via `--use-angle=swiftshader`, `?page=main`)

- **Self-check total.** Before this change (origin/main, `c84f9ae`, same environment): **117/119**
  passing, 2 pre-existing failures (`Specimen underlay` pearl-coverage percentage, `logo styles`
  `elementFromPoint` pointer quirk — both reproduced identically before and after, unrelated to
  B365). After: **118/120** — net +1 row (one control removed, two added, both passing) and +1 pass
  (the removed control was passing; both new rows pass). The same 2 pre-existing failures, unchanged.
- **The five B365 rows, all GREEN:**
  - "the ripple and quiver settle to rest once the sound has ended": every voice freed at 0.98 s,
    output under −90 dBFS from 0.98 s, energy 1 s into the silence ≤ 1% of its sounding peak,
    exactly 0 from 2.05 s onward.
  - "while sound plays the pearl is the pre-B365 pearl, bit for bit": 23/23 sounding windows match.
  - "CONTROL: the pre-B365 Specimen, fed by the monitor, is still rippling at the end": ripple/quiver
    energy 1.180 at 3.5 s (stale-feed bug reproduced).
  - **"B365 correction: the idle motion keeps moving through 2 s of silence"**: summed
    `|Δtime| + |Δyaw|` over `[0.98, 2.98]` s of silence = **2.022** (gate `> 1.0`).
  - **"CONTROL: the merged-B365 law freezes the idle motion in the same silence"**: same measure
    under `S.freezeIdle` = **0.136** (gate `< 0.3`) — well under the fixed law's 2.022, demonstrating
    the merged-B365 bug the human corrected.
- **Idle frame cost, at or below pre-B365.** The "shedding: the Specimen and the logo draw less"
  row reads **bit-identical** before and after this change: "Specimen 20 → 5 draws" over 60 frames
  (calm vs. shedding), both runs. This is not a coincidence — the only code removed was the B365
  skip condition; the draw-rate throttle (`S.n % (LOAD.shed ? SHED_SPECIMEN : 3)`) is untouched, so
  the per-frame draw decision is governed by exactly the same code pre- and post-correction. "At
  rest nothing is redrawn" (B365's optimization) is gone by design (idle motion never rests), but
  nothing makes idle frames MORE expensive than before — same shader, same uniform calls, same
  throttle cadence.
- **Screenshots** (scratch, not committed — session temp dir, captured via the new `b365Shots()`
  at `?page=main&specimen=1&b365shots=1`):
  `/private/tmp/claude-501/-Users-machinepriest-Documents-Claude-synthetic-worlds-HYPERSAW/8f39079a-d2c3-45ac-95ed-20c1077b7b03/scratchpad/b365b/`
  - `b365-idle.png` — idle, no note yet.
  - `b365-held.png` — a note held: visible concentric ripple rings on the surface.
  - `b365-released.png` — ~1.5 s after release: ripple rings gone, silhouette has drifted (a
    different metaball framing than idle) — idle motion continued through the settle.
  - `b365-released2.png` — ~2.5 s after release: still drifting, ripple still absent.

## Alternatives rejected

- **A second copy of `specimenStep` for the merged-B365 control.** Rejected: `S.freezeIdle` (one
  boolean, one ternary) reproduces the exact bug without duplicating ~40 lines that would drift out
  of sync with the real function over time.
- **Driving the screenshots through a real `AudioContext` note-on/note-off.** Rejected: this
  environment cannot drive interactive input into headless Chrome without a new dependency
  (no puppeteer, no CDP/websocket library available); `AUD.feed` already carries exactly the shape
  `feedOf` hands the shader, so setting it directly is the same data path, not a shortcut around it.
- **Keeping `specimenEnergy`'s `U.act` term and just adding a separate idle metric.** Rejected: a
  metric that is supposed to hit exactly 0 in silence must not include a term that is deliberately
  never 0 (the whole point of the correction); keeping it would have made the "settles to 0" row
  either wrong or require the same threshold games the human's correction was fixing.

## Verify

- Committed hash `ebd2019` (branch `lab-specimen-idle`, based on `origin/main` at `c84f9ae`).
- `./verify fast` on the committed hash, exit 0:
  ```
  {"target":"fast","exit":0,"git":"ebd2019","ts":"2026-09-29T22:24:41Z"}
  ```
  Tail: `playbook_check: GREEN (186 citations — 62 at their cited line, 124 drifted (advisory);
  morphLayout 9 at 4 writers; 28 engine sources free of clocks and unseeded RNG)`.

## Open questions

- The two pre-existing headless-environment failures (`Specimen underlay` pearl-coverage
  percentage, `logo styles` pointer quirk) are unrelated to this change and out of scope (not
  B365/B367's aa toggles, not the engine); reproduced identically against `origin/main` before this
  branch, so nothing here introduced or fixed them.
- `b365Shots()` is new, permanent (query-param gated) code, in the same spirit as the existing
  `misreadAll`/`probeAudio` harnesses. If the lead would rather this evidence-capture code not live
  in the shipped file permanently, it can be dropped in a follow-up without touching the fix itself
  (it is inert unless `?b365shots=1`).
