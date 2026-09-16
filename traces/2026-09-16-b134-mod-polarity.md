# b134-mod-polarity — every modulation route gets a polarity (as-is · uni · bi · inv)

- **Queue item:** ROADMAP B134 (human 2026-09-16: *"Important roadmap issue: all
  mods need a bipolar toggle"*). Dispatched to a scoped implementer via
  `briefs/2026-09-16-b134-mod-polarity.md` (authored by the HYPERSAW lead organ,
  2026-09-16, on branch `b131-rn-viz`).
- **Why:** a route could only SCALE its source, so a source's polarity was
  whatever the source happened to be. The setting is per ROUTE and not per
  source because the same modulator drives a filter unipolar and a pan bipolar
  inside one patch — B134's own reasoning, and the reason ORBITAL, B16's LFOs
  and every future source inherit this for free.

## What changed

- `src/mod_core.h` — `Route` gains `int polarity`; `ModCore` gains
  `srcPol[kMaxSources]` (the shell's flag array) and `static mapPolarity(v, pol,
  srcPol)`. `evaluate` maps the source BEFORE depth. `addRoute` takes an
  optional fifth argument defaulting to `kAsIs`, so `tools/mod_check.cpp`'s
  four-argument calls are untouched.
- `src/hypersaw_clap.cpp` — the SOURCE POLARITY TABLE (slot 17 pitch wheel and
  the retired 10-13 XY aliases bipolar; everything else unipolar), installed
  through a member initializer (`mod = makeModCore()`) rather than a call in
  `factory_create_plugin`, so no construction path can forget it. Adds
  `modSetPolarity`, `"pol"` + `"srcPol"` in `modRoutesJson`, the chunk's fourth
  field on write and parse, and two headless hooks
  (`hypersaw_debug_modroutes` / `hypersaw_debug_modpolarity`).
- `src/gui/hypersaw_gui.h`, `src/gui/hypersaw_gui_common.h` — `modSetPolarity`
  on `GuiHost`, bound as `hzModPolarity(routeIndex, pol)`.
- `src/gui/gui2.html` — a 30px cycling cell on each route row beside the source
  select; `modSrcSpan()`; halo reach now computed from the MAPPED source span.
- `tools/polarity_check.cpp` + `CMakeLists.txt` — the oracle, STANDALONE and
  UNWIRED (charter: wiring a gate is the human's decision, proposed in the PR).

## Two decisions worth naming

1. **ADR-138's `(src, dest)` merge is retired.** The chunk canonicalised by
   summing the depths of duplicate `(src, dest)` pairs — sound under the SUM
   law, which made merged and un-merged forms indistinguishable. Polarity breaks
   that identity: `ENV1→detune +0.5 as-is` and `ENV1→detune +0.5 bipolar` do not
   sum to one entry of any depth, so a merge would silently discard one route's
   setting. The chunk now writes one entry per ROUTE; the loader already created
   one route per entry, so the read side is unchanged and B72 can still key on
   `(src, dest)` when it arrives.
2. **The fourth field is OMITTED when 0.** "Bit-inert" then means the bytes, not
   merely the behaviour: a polarity-free patch serialises to exactly its
   pre-B134 chunk. This keeps `tests/state_fixtures` and `gen_state_fixtures`'s
   own `"modRoutes":"0:4:0.5;"` assertion honest without touching either.

**One intentional GUI behaviour change, stated so it is not discovered later:**
the halo's reach used to be `depth × [0,1]` for every route. That was true for
an envelope and a LIE for a bipolar source — a pitch-wheel route drew a band it
could not reach. Reach now comes from `modSrcSpan(pol, srcPol)`. Drawing only;
no audio path touched. Measured spans:

```
  as-is  unipolar src -> [0,1]    bipolar src -> [-1,1]
  uni    unipolar src -> [0,1]    bipolar src -> [0,1]
  bi     unipolar src -> [-1,1]   bipolar src -> [-1,1]
  inv    unipolar src -> [-1,0]   bipolar src -> [-1,1]
```

## Evidence consulted

ROADMAP B134 (read from `origin/b131-rn-viz`, which is where the brief lives);
DECISIONS ADR-136 (base/offset contract, readback reports base), ADR-137 (the
offset is drawn never moved; macros as sources), ADR-138 (route persistence and
the merge this retires), ADR-141 (source lives in the table; the measured
"five across cannot fit" note that shaped the row's layout), ADR-149 (MIDI/MPE
slots 14-17), ADR-152 (macro suspension under morph), ADR-156 (slots 10-13
retired, read 0); `tools/statefix_common.h` (reused rather than growing a third
CLAP scaffold); `src/hypersaw_clap.cpp:2100-2160` (chunk), `:1528` (the `mod`
member), `:2200-2230` (application and OQ-30 clamping).

## Calibration — the check can fail

All-green on a first run is the comfortable answer, so four defects were planted
and each had to turn `polarity_check` red. Harness in a per-stream scratch
directory (L0048), never committed. Object files are deleted before each build:
plant → build → restore → build all land inside one second and make's timestamp
granularity silently reused a stale `.o` on the first attempt, which made every
plant "fail" identically because the tree under test was the previous plant's.
That first run is the reason this paragraph exists.

| plant | failures |
|---|---|
| `evaluate` ignores polarity (the pre-B134 expression) | 8 — all of section A, the B calibration, and the two E cases that reach below base |
| the serializer always writes the fourth field | 3 — exactly the three byte-level chunk assertions |
| the parser ignores the fourth field | 7 — every round-trip plus the two E cases |
| the shell's table calls the XY aliases unipolar | 1 — exactly the assertion that names them |
| restored | PASSED (0 failures) |

Each plant fails its own assertions and no others: the sections are
independently load-bearing, not one assertion wearing five hats.

## Alternatives rejected

- **Polarity per SOURCE.** Cheaper, and wrong for the reason B134 states — one
  patch wants the same modulator both ways.
- **`modRoutesJson` returning an object** `{routes:[…], srcPol:[…]}`. Four GUI
  consumers treat the parse as a list; `srcPol` rides each route instead, which
  costs one integer per row and no shape change.
- **A `<select>` for the polarity cell.** ~14px of dropdown chrome on a row
  ADR-141 had already measured as full. A cycling button carries its state as
  text in 30px.
- **Re-rendering the route list on a polarity click.** The clicked `r` IS the
  `MODROUTES` entry the halo pass reads, so mutating it is the refresh; a
  rebuild would take focus off a control whose whole interaction is repeated
  clicking.
- **A test hook in `src/hypersaw_clap_entry.h`.** Out of scope; `state_check`'s
  precedent of declaring `extern "C"` in the check itself covers it.

## Verify

- `./verify fast` — exit 0 (gated every commit).
- `./verify full` — exit 0. `.harness/last-verify.json`:
  `{"target":"full","exit":0,"git":"fe1808f","ts":"2026-09-16T04:14:38Z"}`
- `polarity_check` — PASSED (0 failures), 29 assertions.
- Run BY HAND because `./verify` does not wire them:
  `statefix_check tests/state_fixtures` → GREEN (3 fixtures, 0 failures) — both
  corpus chunks carry `modroutes=0:4:0.5;` and still render bit-identical, which
  is the byte-level bit-inertness claim measured rather than argued;
  `undo_check` → GREEN (0 failures).
- `node tools/labharness/lab_load_check.mjs src/gui/gui2.html` → GREEN (the
  L0026 TDZ trap; gui2.html is not in the gate's default corpus).

## Open questions

- **`polarity_check` is unwired.** Wiring it into `./verify fast` is the human's
  call. Recommended: it is sub-second and links `-impl` like `state_check`.
- **The GUI was not exercised in a browser.** The row DOM in the PR body was
  built by `renderModRoutes`'s own source lifted verbatim from `gui2.html` and
  run against a ~60-line DOM shim — it proves STRUCTURE (the cell exists,
  carries `data-pol`, cycles the four labels, disables on the pitch route). It
  does NOT prove LAYOUT: whether 72px still reads as a source name and whether
  the four-column line one fits ADR-098's 240px cluster is a human's eye, and
  `MEMORY/browser-audio-probes-are-blind` is the standing warning against
  claiming otherwise.
- **The halo reach change is drawing-only and untested in a browser.** The span
  table above is measured from the shipped `modSrcSpan`; that the band lands
  where the band should land is not.
- **`ROADMAP.md` / `DECISIONS.md` were not touched** (out of scope). The text
  the lead would add is in the PR body.
