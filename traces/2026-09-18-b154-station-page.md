# b154-station-page — the STATION engine page, drawn in gui2's token set

- **Queue item:** B154 (`git show origin/lead-records-43:ROADMAP.md | grep '^| B154'`) —
  "**STATION page prototype in horde's design system** … A design lab
  `docs/design/station-page-lab.html` in gui2's tokens … laid out as the engine's
  PAGE would sit in the shell … and one live visualiser fed by the lab's own core
  extracted at load (the `extract_core` idiom) so the page SOUNDS."
- **Why:** The engine spec (specs/SPEC-STATION.md) and the human's own page mock
  (docs/design-system/HORDE-STATION-Page.dc.html) disagree about *chrome* — the mock
  predates gui2's token set — and agree about *layout*. This lab keeps the mock's
  layout and replaces its chrome with the tokens, so the open question ("what does
  STATION look like inside horde?") is answered without re-opening the settled one
  ("what does horde look like?"). It carries its own DSP so the page can be judged
  by ear as well as by eye, which is the whole difference between a page mock and a
  page prototype.

## Evidence consulted

- `specs/SPEC-STATION.md` §2–§12 (protected; read only). §10 is the parameter table
  the mapping below is keyed to; §11's divergence list is what the core implements
  beyond the prototype.
- `reference/station.html:140-300` (protected; read only) — the parity oracle's DSP:
  envelope constants, LFSR taps, one-sample matrix delay, waveform branches,
  `ALGS` (:463), the wave-RAM generators and pointer paint (:485), `KEYMAP` (:534).
- `src/gui/gui2.html` — tokens `:26-96`, the tube canvas rule `:103`, ORCHID `:142`,
  light-chassis legibility `:167`, dark chassis `:186`, `.cluster` `:345`, the matrix
  well `:404`/`.mxc` `:429`, `.row` `:485`, slider/toggle/enum `:490-560`,
  `VS()/fitViz()` `:1665`, `scan()` `:1678`, `sunset()` `:1682`, `TOK/TOKA` `:1695-1719`,
  the theme-switch cache purge `:1896`, `drawEnvelope` `:5930`, `_drawSpecOne` `:5036`.
- `tools/golden/extract_core.mjs` — the marker-slice + `new Function` idiom.
- `tools/labharness/lab_load_check.mjs`, `tools/gen_lab_index.py` — the two gates a
  lab must satisfy.
- `docs/design-system/HORDE-STATION-Page.dc.html` — read as the human's intent for
  layout and emphasis (`:67` operator row of four, `:158` bottom row), chrome discarded.

## What the page is, exactly

- **Sounds:** the three operators, the PM matrix including self-feedback, phase
  quantization, the pure↔raw crossfade, DRW against the live Wave RAM, the LFSR
  noise channel (both taps, keytrack), all four ADSRs with LOOP and STEPPED, RING,
  hard sync, FREE/RETRIG phase, the pitch envelope, per-op level/pan, master.
- **Mock (drawn, not wired to DSP):** nothing. Every control on the page moves a
  value the core reads. The three things that are *lab affordances* rather than
  engine parameters are marked `data-lab` and named as such on the page: POWER /
  PANIC / theme, the PW snap buttons (§3.2 calls them a UI affordance), the six
  algorithm recalls and five wave-RAM tables (§4/§5: stored patches, not parameters),
  and the on-screen keyboard.
- **Not implemented, and the page does not claim it:** §5's band-limited DRW
  resynthesis (the prototype's linear interpolation stands), §8's 16-voice ceiling
  is enforced but the release-fade is a flat 5 ms rather than a measured one, and
  the ScriptProcessor render is a browser workaround per §11.6. Stated in the file
  header so a future reader does not mistake the lab for a port.

## §10 → control mapping (21 of 21 rows, 90 addressed controls)

`data-addr` carries the §10 ID with `{n}` expanded, in the existing presentation-table
style (`src/param_presentation.tsv`: `osc1.enable`, i.e. `scope.name`). The page
recomputes this table at load and prints the result under the tagline; the run that
produced the screenshots read **§10 rows 21/21 covered by 90 addressed controls; 2
off-table; 50 lab-only affordances**.

| §10 row | address(es) on the page | control | n |
|---|---|---|---|
| `op{n}.on` | `op1.on` … `op3.on` | toggle in the card header | 3 |
| `op{n}.wave` | `op{1,2,3}.wave` | 6 capsules SIN/TRI/SAW/PLS/QTR/DRW | 3 |
| `op{n}.mode` | `op{1,2,3}.mode` | 3 capsules RATIO/PITCH/FIXED | 3 |
| `op{n}.ratio` | `op{1,2,3}.ratio` | slider 0.25–16 **continuous** (§3.1 divergence) | 3 |
| `op{n}.semis` | `op{1,2,3}.semis` | slider ±24 st | 3 |
| `op{n}.fine` | `op{1,2,3}.fine` | slider ±50 c | 3 |
| `op{n}.fixed` | `op{1,2,3}.fixed` | slider 20–4000 Hz | 3 |
| `op{n}.lvl` | `op{1,2,3}.lvl` | slider 0–1 | 3 |
| `op{n}.pan` | `op{1,2,3}.pan` | slider ±1, L/C/R readout | 3 |
| `op{n}.pw` | `op{1,2,3}.pw` | slider .05–.95 + 12.5/25/50 snaps | 3 |
| `op{n}.pure` | `op{1,2,3}.pure` | slider 0–1 | 3 |
| `op{n}.qnt` | `op{1,2,3}.qnt` | enum OFF/4/8/16/32/64 | 3 |
| `op{n}.phase` | `op{1,2,3}.phase` | slider 0–360° | 3 |
| `op{n}.retrig` | `op{1,2,3}.retrig` | enum FREE/RETRIG | 3 |
| `op{2,3}.sync` | `op2.sync`, `op3.sync` | toggle (absent on OP1, per §3.4) | 2 |
| `op{2,3}.ring` | `op2.ring`, `op3.ring` | enum OFF/×OP1/×OP2 | 2 |
| `op{n}.env.*` | `op{1,2,3}.env.{a,d,s,r,loop,step}` | 4 sliders in **ms** + toggle + enum, over a drawn envelope | 18 |
| `ns.on/mode/rate/ktrk/lvl/pan/env.*` | `ns.on`, `ns.mode`, `ns.rate`, `ns.ktrk`, `ns.lvl`, `ns.pan`, `ns.env.{a,d,s,r,loop,step}` | the fourth card | 12 |
| `mtx[src][dst]` | `mtx.{op1,op2,op3,ns}.{op1,op2,op3}` | 12 well cells, drag/scroll/dbl-click | 12 |
| `penv.amt` | `penv.amt` | slider ±24 st | 1 |
| `penv.dec` | `penv.dec` | slider 5–800 ms | 1 |

Off-table (2): `wave.table` (the carpet) and `wave.seed`. §5 makes the Wave RAM
**patch data** and §10 lists no row for it; the page counts them separately rather
than inventing a parameter ID the spec does not have.

## Alternatives rejected

- **Hiding the tuning rows the current mode does not use** (gui2's `data-when`
  idiom). Rejected: the page's whole claim is that every §10 address has a reachable
  control, and hiding three of four tuning rows would make that claim false while the
  audit still read 21/21. They go `.row.ghost` instead — present, dimmed, addressed.
- **Rendering the NS column as disabled `.mxgap` cells.** Rejected: §4 says noise
  receives no PM, so the destination does not exist. gui2:473's rule is that an
  illegal crosspoint is *absent*, not disabled; the 4×3 grid says it by shape and the
  note says it in words.
- **Importing gui2's stylesheet instead of copying the tokens.** Rejected: a design
  lab must open from the filesystem with nothing built. Copy means drift, which is
  said out loud in the file header rather than hidden.
- **Letting the lab share gui2's `defaultState`/core.** Rejected: there is no STATION
  core in `src/` yet (grep: no match) — this is a page prototype ahead of the port.

## Two findings for the port

1. **`extract_core.mjs` cannot read `reference/station.html`.** The extractor keys on
   `/\* =+ DSP:` … `/\* =+ Audio graph`; station.html's banner is
   `/* ===== STATION — 3-op PM + LFSR noise ===== */` (`:141`) with a plain
   `/* ---------------- DSP ---------------- */` at `:172` and **no** Audio-graph
   banner at all, so both searches fail. Every other prototype carries the pair
   (e.g. swarmsaw.html `:203`/`:670`). Not fixed here: `reference/` is protected and
   an edit there is a spec change. The lab's own block uses the extractor's markers,
   so the idiom is demonstrated and the reference gap is reported rather than
   patched around.
2. **Envelope units.** §7 states A 1–2000 / D 5–3000 / R 5–4000 ms; the prototype's
   defaults (3/420/260 etc.) sit inside those ranges, so the page ships the spec's
   ranges with the prototype's defaults and no reconciliation is needed.

## Verify

- `./verify fast` — **exit 0**, git `35c4c4e` (`.harness/last-verify.json`).
- `node tools/labharness/lab_load_check.mjs docs/design/station-page-lab.html` —
  `OK station-page-lab.html … GREEN — 1 labs loaded, 0 broken, 0 skipped`.
- `python3 tools/gen_lab_index.py` — `wrote docs/design/index.html with 23 lab(s)`.
- **Headless core probe** (scratch, not committed): the SHIPPED
  `tools/golden/extract_core.mjs` pulled `StationCore` out of the lab file unmodified
  and rendered 0.5 s from the boot patch — rms 0.120477, peak 0.301508, identical
  across two runs (§12 replay determinism); noise-only 0.025107, DRW 0.152817,
  raw+QNT8 0.089298, ring 0.122646, loop+step 0.122456. **Control:** all levels at 0
  read rms 0.000000 / peak 0.000000 exactly — the must-read-zero case, without which
  the six positive numbers would only prove that *something* renders.
- **Screenshots:** Chrome headless, `--force-device-scale-factor=2`, `file://`,
  1500 px wide, both themes (`?theme=dark` deep link). Light reads the ORCHID screen
  pairing, dark the TUBE — the pairing gui2:1654 ships.

## Open questions

- **The page sounds, but only the DSP is verified here.** The core is proven by the
  headless probe; that it reaches a speaker through the ScriptProcessor is verified
  only by a human opening the file (a headless browser has no output device, and an
  analyser reading silence is indistinguishable from a render callback that stopped —
  which is why the page now counts and displays its render blocks). Ask what it
  sounds like; do not infer it from the picture.
- **Card height.** At 1500 px the four operator cards run ~1000 px tall. In the shell
  the page has the plugin's height, not the browser's; whether the envelope block
  folds (gui2's `wireFolding`) is a layout ruling for the port, not a lab question.
- **Continuous RATIO with no snap affordance.** §3.1 says snap-to-0.5 is a UI
  affordance; the page ships the continuous slider without it. Add the snap when the
  human has heard the glide, not before.
