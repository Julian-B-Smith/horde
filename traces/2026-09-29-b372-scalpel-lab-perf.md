# b372-scalpel-lab-perf — the SCALPEL lab profiled; four low-risk page fixes, the large costs ranked for a ruling

- **Queue item:** B372. I read the row verbatim from `origin/lead-records-148:ROADMAP.md`. The human, 2026-09-29: "The scalpel page is really starting to slow down."
- **Why:** The brief was to measure first, then fix only the clear, low-risk wins in `docs/design/scalpel-interface-lab.html`, each measured before and after, and to rank the rest for a ruling. The engine's DSP is out of scope. Headless Chrome shows the page is not dropping frames: 60 fps in every case, and 2.5–5.3 ms of main-thread work per frame. Two things measure as "slow". Every load freezes the page for about 20 s in the self-check. The worklet goes over budget on dense chords. Neither is a low-risk page edit, so both are ranked for a ruling, with numbers. Four small page-side costs were fixed.
- **Evidence consulted:** the B372 row; the lab's sections D (monitor), E/E2/E3/E4 (tokens, logo, Specimen, envelope), F (painters), J/J2 (audio, load loop), K (self-checks), L (frame loop, boot); `docs/design/scalpel-horde-engine.js` (read only); `reference/scalpel/prototype/razor-core.js` (read only); `h2/cores/scalpel/razor_core.h` (header, includes); `docs/port/scalpel-phase-1a.md` (its CPU table); `tools/serve_labs.py`; traces b323 (the method: a CDP response rewrite for the "before" build, interleaved runs) and b365.

## Method

- **Browser.** Headless Chrome (installed Google Chrome, new headless) over CDP, with dpr 2 forced, a 1400×1000 window, and the real ANGLE-Metal GPU on an Apple M3. The server was `tools/serve_labs.py 8372`.
- **Before and after.** The "before" build is origin/main's lab (bc729a8), served by a `Fetch.fulfillRequest` rewrite. Runs were interleaved before/after, because the machine's load drifts.
- **Instrumentation.** All of it went in over CDP only; nothing was written into the tree:
  - `addScriptToEvaluateOnNewDocument` counters for rAF, intervals, listeners, long tasks and long animation frames;
  - `Runtime.evaluate` timers around the frame's parts and around every registered painter;
  - a 100 µs sampled `Profiler` run;
  - `Performance.getMetrics` for layout and recalc counts and durations, the heap after a forced GC, nodes and listeners.
- **The worklet.** Its own meter (`LOAD.log`) and `playbackStats`. Separately, the composed engine timed per 128-sample block in Node 24, with the exact params the page posts (`oracleParams(EFF)`, captured from the page), seeded as the worklet seeds it, and with no Chrome running.
- **The Specimen's GPU.** Bounded by a forced draw plus a synchronous 1-px `readPixels`, with a readPixels alone as the control.

## Measured (before)

- **Boot self-check.** `runChecks` runs synchronously, once, before the frame loop starts: **20.4 s**. Boot to checks published: 19–24 s.
  - The heavy checks: checkB366 4.0 s, checkInterplay 4.0 s (3.8 s of it is 14 `misreadRun` calls in `interplayKernel`), checkRotation 3.3 s, checkB323 2.6 s, checkB334 1.8 s, checkB365 1.2 s.
  - The checks also call `rebuild()` 120 times (0.95 s).
  - Its result block is 2,668 px tall and pushes the plugin frame to ~3,000 px down the page.
- **Worklet.** The page's meter with a five-note chord:
  - default patch: 55–78 %;
  - Glass horde pad: **115–145 %**, with **1,601–1,604 underruns in 8 s**.
- **Engine in Node, per 128-sample block (budget 2.667 ms).**

  | Patch | 1 voice | 5 voices |
  |---|---|---|
  | default | 0.375 ms | 1.95 ms (73 %) |
  | Glass horde pad | 0.84 ms | **4.0 ms (150 %)** |
  | Crushed bells | 0.59 ms | 2.65 ms (99 %) |

  - The composed engine costs 1.4–2.2× the oracle class alone per voice.
- **Main thread, per frame** (idle, then playing):
  - the monitor (the second engine instance): 1.4 ms idle, 2.8–4.2 ms playing;
  - the canvas painters: 0.4–1.5 ms;
  - the logo warp: ~0.7 ms (`render :3342`, the top page-side function, at 4.1 % of wall);
  - the Specimen's CPU side: under 0.1 ms. Its GPU time is 0.9–1.7 ms per draw at 504×248 device px (control readPixels 0–0.1 ms), at ~20 Hz.
  - GC: 0.8–1.3 % of wall.
  - 0 long tasks and 0 frames over 20 ms in every 8 s run after boot.
- **OSC layout thrash.** The page ran **64–82 layouts per second**, i.e. about one per frame, costing 15–17 ms/s. `fit()` carried 18.7–22.9 ms/s of self time.
  - Cause: `paintSpectrum` wrote `cv._note.textContent` unconditionally every frame. The next painter's `fit()` then read `clientWidth` and forced a synchronous reflow.
- **Viz-feed traffic.** The worklet posts a Specimen feed every 16 blocks (23.4/s) and a load window every 32 blocks (11.7/s). Each is handled in ≤ 0.01 ms/frame.
- **Memory and leaks.**
  - Heap 5.4–7.8 MB after GC over 60 s of play, and across 20 page, 30 skin and 10 theme switches and 8 logo clicks, with no trend.
  - 3,101 DOM nodes and 10 canvases throughout.
  - One rAF loop, no intervals.
  - The one leak was listeners on POWER on/off: 20 pairs took them from 381 to 401.
  - 20 closed AudioContexts and 20 AudioWorkletNodes stay reachable in both builds. A control page with no lab code shows the same (5 closed, unreferenced contexts stayed reachable, and 10 with a connected gain), so that part is Chrome's.

## What changed (commit 47f5644, `docs/design/scalpel-interface-lab.html` only)

1. **`paintSpectrum`** writes its note only when the text changes.
   - OSC idle, frame in view: 64.3 → 3.3 layouts/s, 15.6 → 0.5 ms/s of layout, `fit()` self time 18.7 → 1.0 ms/s.
   - The carpet painter, which paid for the reflow, went from 0.36 to 0.07 ms/frame.
   - Playing: 76 → 14 layouts/s.
2. **`paintLoad`** writes the meter's text, bar width and title only when each changes. MAIN playing: 15.8 → 13.9 layouts/s. What remains is real text changes.
3. **The frame loop skips the canvas painters while the whole plugin frame is outside the window** (`paintCanvases(true)`, `frameShown()`).
   - At the load position: 0.40–0.83 → 0.007–0.009 ms/frame.
   - With the frame in view the cost is unchanged: one `getBoundingClientRect` per frame.
   - The unit is the frame, not each canvas, because `paintSpectrum` and the ring write DOM text beside their canvases, and a per-canvas skip could leave that text stale on screen.
   - `paintEnv` and `paintVmap` always run, because each keeps state across paints: `ENVV`'s first release level, and the voice map's 30-paint shrink hysteresis.
   - `rebuild()` and every check still call `paintCanvases()` with no argument, which paints them all.
4. **POWER off** nulls and closes the node's port and disconnects the node, the fallback's ScriptProcessor and the gain before closing the context. `makeNode` revokes its blob URL once `addModule` has resolved. 20 on/off pairs now keep the listeners at 381 (they went to 401 before).

**The self-check, before vs after.** Both pages read 120/120 → 120/120. Row names, order and ok flags are identical. Only two rows' detail text differs, and only in measured timing numbers: the live DSP % in the load-meter row, and the per-style ms (and which style is heaviest) in the logo row. The brief quotes 118/120 on main in headless. With the real GPU and dpr 2 used here, both builds read 120/120.

## Alternatives rejected

- **Hoisting the logo warp's per-pixel `Math.sin(t·ω)`.**
  - It was byte-identical over 400 frames, but the must-fail control (a deliberately reassociated sum) also read 0 bytes different. The byte detector is too coarse to prove bit-identity for this term.
  - Timing: 1.82–1.99 ms vs 1.83–1.98 ms per render across 6 interleaved pairs, so no gain. V8 already hoists it.
  - Reverted.
- **Skipping each canvas separately when it is off screen.** Rejected for the stale-text reason above.
- **Skipping the logo and the Specimen off screen.** Both animate on engine-clock dt with clamps, so a skip changes their phase on return. Left for the ruling list.
- **Gating the self-check behind a parameter.** It is the largest gain, but it changes the page's contract with the agents that read `window.__b271` after a headless load. It needs a ruling.

## Ranked for a ruling (gain against risk)

1. **The 20 s boot self-check.**
   - Options: (a) run it on request; (b) run it chunked between frames, with input held; (c) move the six heavy engine-level checks into a Node labharness gate wired in `verify`.
2. **The worklet over budget on dense chords.** Engine scope. The options are a WASM core after the composed engine is ported, an oversampling choice, a cap that may thin HELD voices, or re-voicing.
3. **The main-thread monitor.** It is 50–80 % of the page's frame work.
4. **The logo warp** (~0.7 ms/frame). Options: port it to WebGL, or use a smaller bitmap.
5. **Skipping the logo and the Specimen off screen.**
6. **Per-frame allocation churn.**
7. **Dirty flags for canvases that have not changed.**
8. **`rebuild()` at 8–10 ms per enum or toggle click.**
9. **The 2,668 px self-check block above the frame.**

## WASM note (analysis only)

- **The toolchain is missing.** Apple clang 16 has no wasm32 target: `--target=wasm32` fails, and `-print-targets` lists only AArch64 and x86-64. There is no `wasm-ld`, `lld` or `emcc` on the machine.
- **What it would need.** Either Homebrew `llvm` + `lld` with wasi-sdk's libc/libm, or Emscripten. Either is a new dependency, so it is a human gate.
- **What it might buy.** `razor_core.h` needs only libc/libm headers and allocates nothing in `render()`. The measured native C++ speedup over the JS oracle is 2.3–4× (`docs/port/scalpel-phase-1a.md`). WASM is typically below native, so perhaps 1.5–3×. That is a hypothesis, not measured.
- **But it is the oracle.** `razor_core.h` is `RazorCore`, not the composed engine the lab plays, and the swarm half is 30–55 % of the composed engine's per-voice cost. A WASM build therefore needs B332 phase 1b (porting the composed engine) first, plus a tolerance ruling for the lab's drawn-equals-heard rows, since libm differs from V8's.

## Verify

`./verify fast` on 47f5644: exit 0, GREEN. `.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"47f5644","ts":"2026-09-30T00:47:03Z"}`. No lab gate timed out (B362 did not trigger). Chrome was not running during verify.

## Open questions

- Which self-check route: (a), (b) or (c)? Option (a) would change the lead's headless recipe.
- Is a 60 Hz headless M3 representative of the human's machine? Its compositor and raster time are not visible to headless.
- The per-frame main-thread numbers drift ±0.3 ms between runs of the same build. The counts are exact: layouts/s, listeners, underruns, and the self-check rows.
- The report with every table is in the session scratch, not tracked (`b372-report.html`).
