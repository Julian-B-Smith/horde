# b377-envelope-lab-r2 — the envelope lab answers at once, and every stage gets a shapeable curve

- **Queue item:** B377, read verbatim from the local `lead-records-150:ROADMAP.md` (the branch was not on origin yet; `origin/lead-records-149` was read for B366, B370, B372 and B375).
- **The human, verbatim:** "The envelope hierarchy lab isn't working; it's very laggy and it takes forever for a button to click or a sound to play. But I want the new envelope system to have shapeable curves (with a direct click-and-drag UI in addition to knobs where possible) at every stage in the envelope."
- **Why:** measured first. The page froze for 21 s at every load, and for about 21 s after ANY pill click. The cause was one card: D13 re-measured its tanh curve on every full repaint (2 × 48,000 plotted points × 2 passes of tanh over a 24,000-sample stem = 20.2 s). Renders ran on the main thread too. Curves are a horde 2 requirement, so the lab prototypes them in its harness, as B370 prototyped its laws.

## Method (B372's)

- Headless Chrome over CDP. Dpr 2, a 1400×1000 window, served by `tools/serve_labs.py 8377`.
- The "before" is origin/main's lab (bae777f), served by a `Fetch.fulfillRequest` rewrite. Nothing was instrumented in the tree.
- Instrumentation went in through `addScriptToEvaluateOnNewDocument`:
  - long tasks;
  - Event Timing (`click` duration = input to the next paint);
  - a hook on `AudioBufferSourceNode.start`, which gives click to first audio.
- Clicks are real `Input.dispatchMouseEvent`s.
- "Cold" runs remove `IntersectionObserver`, so nothing is prefetched.
- Chrome was never running while `./verify` ran.

## Latency: before and after

| | before (bae777f) | after |
|---|---|---|
| boot: longest main-thread task | 21,077 ms (total blocking 21,369 ms) | 102 ms (total blocking 139–208 ms) |
| boot to quiet | 21.2 s | 1.9–2.3 s |
| a click 300 ms after load | lands in the 21 s task | answered in < 16 ms |
| header pill (the law) → response | about 21 s (the full repaint) | 16–48 ms |
| any ▶ → response (click to next paint) | 24–136 ms | 16–32 ms |
| ▶ A, Pluck · sync blade, uncached → first audio | 303 ms | 174–205 ms |
| ▶ B, same preset (stems shared) | 663 ms | 467 ms cold · 1 ms once prefetched |
| ▶ blind, Pad row, uncached | 5,068 ms | 3,846 ms cold (2.5 s when a prefetch was already running) |
| ▶ A then B, Swell row, uncached | 2,057 ms | 1,509–1,556 ms |
| §3 ▶ option 1 / 4, uncached | 355 / 334 ms | 303–328 / 290–319 ms |
| any cached phrase | 22–29 ms | 0–4 ms |
| §2 in view for 40 s, then every §2 ▶ | (no prefetch) | 1–4 ms (26 phrases, 61 MB, cached in the background) |

- **Where the time went, per repaint (before):**
  - paintD13: 20,232 ms;
  - §3, cold: 571 ms (the evidence simulations about 500 ms of it);
  - §2's law runs: 246 ms, every time;
  - the boot self-check: 398 ms.
- **An uncached render still costs what the engine costs.** A pad phrase takes about 2.5–3.8 s. It is no longer on the main thread, and the button shows `rendering N%` from the click's first frame, as does the header's bar ("rendering Pad · slow bloom · A (SCALPEL) 23% · …").

## What changed (`docs/design/envelope-hierarchy-lab.html` only)

1. **D13** is measured once, at 121 levels, and kept. The dB readouts are still exact at S 0.5.
2. **lawTimes** is memoised on (shape, times, rate, gate).
3. **Simulations** are cached across paints, with a generation sweep.
4. **The paint is split.**
   - Light painters run in the frame: the editor, the flow and the hold.
   - Heavy ones (§1 lifetime, §4, §2 per row, §3 cards, the evidence warmed two simulations per task, §5) run 160 ms after input settles, each in its own task.
5. **THE WORKER.**
   - It loads the page's own `<script src>` URLs, then the page's first inline script (the core block A) verbatim, and runs the same `mixGen`.
   - A priority scheduler runs ~25 ms slices. A click preempts a prefetch. A preempted job's `Math.random` (the engine's seeded shim) is saved with it and restored when it resumes.
   - B370's main-thread slicer remains as the fallback when no Worker is available.
6. **The mix cache** keeps finished mixes per scenario JSON (up to 200 MB, LRU) and keeps each AudioBuffer with its mix. **Prefetch** covers the sections on screen, at idle.
7. **The AudioContext** is created on the first pointerdown anywhere, which cost about 90 ms on the first click before.
8. **The self-check** runs on `?check=1` or §6's button. `window.__b377` and `data-selfcheck` are set only then (B375's rule).

**The render route is bit-identical (proved two ways).**
- (a) Self-check WORKER: the worker's mix equals a main-thread `runSync(mixGen)` for a full-stem phrase and a member-stem one: 0 of 347,898 samples differ. The control is a scenario one part in a million away: 38,953 differ.
- (b) Scratch, end to end: all 47 ▶ of the page (the `?smoke=1` list) were rendered by the BEFORE build (main thread, B370's two-law `Env`) and by the AFTER build (worker, the curve family's presets). FNV hashes of L and R: **47 of 47 identical**.

## Curves: the family

- **One recursion runs every moving stage:** `e ← e + (aim − e)·κ`, where `κ = 1 − e^(−k/N)`, `k = 8c` and `N = T·sr`.
  - Here c ∈ [−1, +1] is the stage's curve.
  - The attack spans 0→1. The decay runs from its entry level to S. The release runs from the key-off level to 0.
- **"Time to finish"** lands exactly at T: `aim = y1 + (y1 − y0)·e^(−k)/(1 − e^(−k))`, that is `y(t) = y0 + Δ·(1 − e^(−kt/T))/(1 − e^(−k))`.
  - c 0 is the straight line: `e ← e + Δ/N`.
  - c > 0 starts fast; c < 0 is convex.
  - The landing step is clamped onto y1.
- **"Time constant"** aims AT y1 and never lands, so T holds k time constants.
  - c is floored at +1/64.
  - The attack hands over at 0.995. The release frees under 1e-4.
  - ADR-021's AR path (S ≥ 1, no hold) is kept.
- **Hold is flat, so it has no curve** (see the open questions).
- **The presets:**
  - **SCALPEL:** attack c 0 as a time to finish; decay and release c +0.5 as time constants (k 4). SCALPEL's own law mixes the two meanings.
  - **horde:** A, D and R at c +0.125 as time constants (k 1).
- **N keeps the oracle's ms round trip, `T·1000·0.001·sr`,** so the SCALPEL preset is the engine's bits.
- **§0 is the editor.**
  - The drawn envelope has handles: ● time nodes, ■ the hold tab, the sustain ● (time and level), and ◆ (or anywhere on a segment) to bend that segment.
  - There are ten knobs: A, H, D, S, R and the curves of A, D and R. The hold and sustain cells say why they have no curve.
  - It has TIME MEANS, RESET TO SCALPEL's/horde's curves, and ENV 1–4.
  - Drags and knobs write through one set of setters. Double-click resets. Alt + wheel on the knobs goes through `onAltWheel`.
  - A stage indicator and a ring follow the rendered mix at the audio clock.
- **§2 is decision 1 in curve terms.** A and B are the two presets, and a new **▶ yours** plays ENV 1's §0 curves on each preset's times. The header pills set all four envelopes to a preset.

## Self-check (`?check=1`): 46/46, 23 must-fail controls

- **SR:** the family is the same in seconds at 44.1 and 48 kHz, for curves −1…+1 (finish) and +1/64…+1 (tau). Worst Δ 22.7 µs.
  - CONTROL: frozen at 48 kHz, off by 24.76 ms.
- **PRESETS:** both presets are B370's laws (kept verbatim as `LawEnv`), sample for sample. 184 runs (23 envelopes × 2 laws × 44.1/48 kHz, a legato retrigger, the r× clock), 51,455,719 samples, **0 differ**.
  - CONTROLS: curves one grid step away (12.7 M differ); the same curves read as a time to finish (13.4 M differ).
  - Horde's κ agrees with ADR-021's `T·sr` on all 40,000 whole-ms times.
  - CONTROL: off the grid, 39 of 100,000 random times round apart (the claim is scoped).
- **NEUTRAL:** curve 0 is the straight line. 0 samples off the accumulator; at most 3.4e-12 from `y0 + Δ·n/N`; lands within 1 sample of T.
  - CONTROL: c = 1/512 departs by 1.95e-3.
- **MONO:** 579 envelopes (129 finish curves and 64 tau curves, each at 3 times) show 0 samples turning back or overshooting.
  - CONTROL: the attack without its landing clamp overshoots.
- **DRAG=KNOB:** a drag on the attack segment and the knob write the same state (c 0.19921875 both, state identical, 0 curve samples differ). Each view shows what the other wrote.
  - CONTROL: a view writing a private copy leaves the model unchanged.
- **DRAWN:** the editor's curve is the harness's record, sample for sample: 232,004 samples, 0 differ.
  - CONTROL: horde drawn as if its knob times were times to finish differs in 120,000 of 120,000.
- **IND:** at the attack knob's time, the indicator reads ATTACK at the rendered level 0.6321.
  - CONTROL: a knob-time model reads DECAY there.
- **WORKER:** described above.
- **B370's 26 rows are unchanged.** HOLD0 now proves the SCALPEL preset equals the engine's `v.env`: 0 of 57,441 differ.

## Screenshots (scratch, not tracked)

In the session scratchpad's `b377/` folder:
- `shot-editor-light.png` and `shot-editor-dark.png`: the editor with handles. The attack is bent convex (−0.55), the release steep (+0.70), and the decay +0.30.
- `shot-editor-horde-dark.png`: horde's defaults.
- `shot-knobs-light.png`: the knob set.
- `shot-decision1-ab.png`: decision 1 in curve terms.
- `shot-progress-header.png` and `shot-progress-row.png`: a cold render at 23–24%.
- `shot-indicator.png`: DECAY lit, the ring on the curve, "level 0.799 · read from the rendered mix".

## Evidence consulted

- ROADMAP B366, B370, B372, B375, B377.
- `traces/2026-09-29-b370-envelope-hierarchy-lab.md`, and `traces/2026-09-29-b372-scalpel-lab-perf.md` on `origin/lab-scalpel-perf`.
- `tools/labharness/lab_load_check.mjs` (it runs plain `<script>` blocks only, so the core block keeps no attribute).
- `tools/labharness/lab_wheel_scroll_check.mjs` (fake DOM: Worker stubbed, timers never run).
- `tools/serve_labs.py`, and LIBRARY L0016, L0026, L0033 and L0064.

## Alternatives rejected

- **A curve family where SCALPEL's law is one global time meaning.** It cannot be done: its attack finishes and its D and R are asymptotic. Per-stage meanings with an envelope-wide switch show the mix honestly.
- **A time base in seconds (`T·sr`).** SCALPEL's linear attack would miss the engine's bits on 7% of whole-ms times.
- **A curve on HOLD.** It would be an inert parameter.
- **Moving §3's simulations into the worker.** Chunking and caching them brought every task under about 100 ms, with less new machinery.
- **A test hook to switch prefetch off.** The scratch removes `IntersectionObserver` instead; no debug flag is committed.

## Verify

- **d5af0ea (the lab):** `./verify fast` exit 0, `{"target":"fast","exit":0,"git":"d5af0ea","ts":"2026-09-30T04:45:53Z"}`.
  - It ran on the committed hash, not chained to the commit, with no Chrome running.
  - `lab_wheel_scroll_check: GREEN — 0 failure(s); 7 labs, 2 planted faults`. The lab's rows: plain wheel over 42 elements with 0 changes; alt wheel moved 40/42; 2/2 onAltWheel call sites (knob, slider).
- **This trace's commit** is re-run on its own hash. The result is in the PR and the handback.

## Open questions

- **HOLD's curve.** The row says "every stage (attack, hold, …)", but a hold does not move, so there is nothing to bend. Built as flat, with the knob cell saying so. Is a hold curve meant as something else, for example a slope or droop to a hold level? That would be a new parameter.
- **Decision 1 is now two questions:** which default curves, and what a time knob means. SCALPEL's law mixes the meanings; horde's is time constants. The human can hear a third answer (all "time to finish" with bends) on ▶ yours.
- **Off the millisecond grid, the preset time base** differs from ADR-021's `T·sr` by one rounding of κ, at about 4 in 10,000 times. It matters only for a C++ port, which would choose one base.
- **The editor's time axis is linear,** so a 1–4 ms attack is a sliver at the left edge; the knob is the precise way to set it. A per-stage zoom was not built.
- **An uncached pad phrase still takes about 2.5–3.8 s** of engine render time; B372's engine-scope items would reduce it. Prefetch hides it once the section has been on screen.
- **`docs/design/index.html`** is not regenerated.
