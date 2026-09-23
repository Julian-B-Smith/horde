# Units, tapers and tempo divisions: Phase 0 inventory (B213)

**Commit inventoried:** `origin/main` at `fb64f70` (PR #720 merged). The B213 row itself is in PR #721 (`lead-records-81`). It is quoted from there, not from main.
**Brief:** the horde lead, 2026-09-22, B213 Phase 0 only. This is a read-only inventory. **Nothing in the plugin changed.** No file under `src/`, `specs/`, `reference/`, `tools/` or `./verify` was edited.
**What this document is for:** the human rules on D1–D6, and those rulings rest on the evidence here. Every PROPOSED cell is a proposal and is conditional on those rulings. Nothing below has been decided.

**How the evidence was gathered.** Each item is labelled as one of three kinds:
- **[probe]** is a runtime observation. Three scratch probes (not committed, see Appendix A) link `libHYPERSAW-impl.a`. They instantiate the plugin through its CLAP factory and read what a host reads, which is `params.get_info`, `value_to_text`, `text_to_value` and `get_value` after a `flush`. `paramclass_check` was also built and run to get the morph class.
- **[code]** is a file:line citation, all at `fb64f70`.
- **[entailed]** follows from cited code but was not observed in a host or a running GUI.

---

## 0. Headline findings

1. **Most of the host's unit text reaches oscillator 1 only, and two of its branches never run.** [code+probe] `params_value_to_text` (`src/hypersaw_clap.cpp:8427-8495`) tests the ids with `id ==` and not with `baseIdOf(id) ==`. The envelope, dissolve, drift-depth, drift-rate and phase-lag rules therefore reach **oscillator 1 only**. Osc 2's Attack reaches the host as `0.003`, while osc 1's reads `3.0 ms`. The octave and semitone branches (`:8458`, `:8462`) are **dead code**, because the `d->stepped` branch at `:8439` catches them first. Octave reads `1`, never `+1 oct`.
2. **Typed entry does not round-trip.** [probe] `text_to_value` is `atof` (`:8512`). The host's own display of the attack default is `3.0 ms`, and typing that back gives **3** (seconds, then clamped to 2 s). `250 ms` gives 250. `2 kHz` gives 2 Hz. `1/3/beat` gives 1. Label entry also returns the **index**, not `minV + index` (`:8508`), so on Poles q (min 1) typing `4 — quad` gives 3, which is the `3 — triad` value.
3. **gui2's own readout drops the unit and mis-rounds short times.** [code] The only writer of a knob's `<output>` is `displayValue` (`src/gui/gui2.html:2319-2321`): `(+v).toFixed(2)`, with no unit. `fmtVal` (`:2303`) feeds only a hint tooltip (`:3128`). **So a 3 ms attack reads `0.00`, a 6 ms attack reads `0.01`.** 74 of the factory bank's 82 attack values are ≤ 6 ms. The legacy `gui.html` had an ms/s rule (`gui.html:522`), and gui2 did not inherit it.
4. **Three rows say `log10` in the table but render linear.** [code] `tools/gen_gui_controls.py:289` emits `data-log10` only when `min > 0`. Glide (33), Morph Glide (158) and Onset Scatter (91/1091) all have min 0, so the generator falls back to linear and says nothing. **This is D1 in miniature: log10 cannot reach zero, and the generator already chooses linear when it has to.**
5. **The ranges already match (a D5 fact).** [probe] ENV 2 (pitch), ENV 3, ENV 4 and the S.envelope carry exactly ENV 1's ranges: A 0.001–2, D 0.005–4, S 0–1, R 0.005–8. They differ from ENV 1 in taper (linear against log10 in gui2) and in host text (bare against ms/s). The one odd one out is the sub (A 0.0005–0.5, R 0.002–2).
6. **The mod-depth effect in the D6 wording is reversed.** [code+arithmetic] A route's depth is a fraction of the **linear** range (`src/hypersaw_clap.cpp:3881-3891`). On an attack at 5 ms, depth **0.01** moves it to **25 ms** (5×), and depth 0.25 moves it to 505 ms. Linear-range depth **over-moves** short times. It does not "barely move" them. In the control's log space, depth 0.25 would move 5 ms to 33 ms. See §D6.
7. **Two opposite "beats" conventions, both labelled `/beat`.** [code] Grid Cycles/Beat (23) and Step Grid (148) are *cycles per beat*: a bigger number is faster. `lfoNBeats` and `dNbeats` are *beats per cycle/repeat*: a bigger number is slower (`:3822`, `delay_core.h:229-230`). The presentation table gives all four the unit `/beat`, which is wrong for the LFO and delay rows. gui2's own LFO scope caption says the opposite ("1 cycle = N beat(s)", `gui2.html:6933-6939`). `kGridSteps` also names 0.25 `"1/4"`. That means 4 beats per cycle, a whole note, while in an LFO division list "1/4" means 1 beat.
8. **Osc 2's Grid Cycles/Beat is not quantised.** [probe] Writing 0.9 to id 23 reads back 1. Writing 0.9 to **1023** reads back **0.9**. The snap is `if (id == 23)` (`:6680`).
9. **Snapping would change nothing in the factory bank.** [probe over files] Only 2 beats values in the 41 patches and 4 corner files differ from default: Delay Dotted `d1beats = 0.75` (dotted 1/8, with sync on) and Tempo Grid Lattice `beatMult = 2`. The 7 state fixtures hold defaults only. Every one of these values sits exactly on its list: B208's for delay/LFO beats, `kGridSteps` for the grids.
10. **No factory patch morphs or modulates a time, frequency or beats parameter.** [probe over files] Across the 41 patches, no morph corner differs on any time/frequency/beats id, and no preset stores mod routes. So neither D3 nor D6 would change how a factory patch sounds. User patches are unknown (§4).
11. **Host automation lanes are linear over min..max, and nothing in the stack can say otherwise.** [code] CLAP 1.2.10 has no taper field (`libs/clap/include/clap/ext/params.h:136-206`). clap-wrapper v0.15.1 normalises VST3 linearly (`src/detail/vst3/parameter.h:60-74`) and sets no AU `Display*` flag (grep: none in `src/`). In an attack lane, the bottom 0.45 % of the height covers 1–10 ms, 10 % of the height is already 201 ms, and the default (3 ms) sits at 0.1 %.
12. **Widening a range (D5) is not free on VST3/AU.** [entailed] Our state stores seconds, so presets survive any widen. But a VST3 host automates in normalised units, and the wrapper maps normalised to plain through `min + x·(max−min)` (`parameter.h:60-66`, applied at `process.cpp:371`). A changed min or max therefore moves every existing VST3/AU automation point, a consequence the row did not count.
13. **"Attack (s)" means two different things.** [code] For ENV 1, the S.envelope and ENV 2–4, the number is a one-pole **time constant**: 63 % at 1·τ (`force_core.h:61-64`, `hypersaw_clap.cpp:2797-2801`). For the sub it is a **linear arrival time** (`subosc_core.h:366-380`). B208's lab (branch `lab-modulator`, `015b612`) proposes arrival time plus curvature, migrating old patches by τ × ln 100. A unit rule (D4) that prints "5 ms" should first know which of the two it is printing.

---

## 1. The three (in practice, five) places a unit lives

| # | Location | What it holds | Coverage |
|---|---|---|---|
| 1 | `kParams` / `kSubOscParams` name strings (`src/hypersaw_clap.cpp:174-776, 1238-1308`) | unit inside the host-facing name, e.g. `"Bend Time (ms)"` | 57 of the 266 continuous ids (twins included) carry a parenthesised unit in the name (`(dev)`, `(cont.)` excluded) |
| 2 | `params_value_to_text` (`:8427-8495`) | hand-written id list; else `%.3f` | 11 id-specific branches: 2 dead, 6 osc-1-only (§3); every Hz, every delay time, every LFO rate, every beats value and every sub row prints bare |
| 3 | `src/param_presentation.tsv` `unit` + `scale` | presentation truth for gui2 | unit filled on **56 of 257 knobs**, 24 more carry it only in the label, 1 label is `(dev)`, **176** have neither; `log10` on 30 rows. (The row's "201 have none" counts the label-only ones as none: 176 + 25 = 201.) |
| 4 | gui2 markup | generated rows copy `unit` into `<span class="u">` and scale into `data-log10`; **hand-placed proxies copy them by hand** | 40 hand-placed range inputs sit outside every `GEN:` block (ENV 1 proxy with log10 at `gui2.html:1287-1305`, ENV 2 proxy linear at `:1314-1317`, the mixer strips `:1162-1205`, the OSC pitch rows `:1700-1703`) |
| 5 | gui2 / gui.html JS | `displayValue` (gui2: unitless `toFixed(2)`); `gui.html:518-547` a 27-id hand list; gui2 `:2815,:2825` hard-code `' st'` for id 38 | See §3 |

---

## 2. The inventory

Every continuous parameter is here. That is 266 ids: 172 base or global, 54 osc-2 twins, 11 sub and 29 routing. Twins and slots are collapsed into one row, and every id is listed. Stepped parameters appear only where they are a sync partner.

**Column key.**
- **Unit: name / host / pres / gui2**:
  - *name* is the unit parsed from the CLAP name string.
  - *host* is the unit `value_to_text` prints.
  - *pres* is the presentation table's `unit` (with `+log10` if `scale` is set).
  - *gui2* is `[u]` for a `<span class="u">` unit, `(x)` for a unit that exists only inside the label text, `—` for none, and `n/c` for no gui2 control.
- **Taper today**: `log10` (gui2 `data-log10`, which exists only in the GUI) or `lin`. The host is **always linear** (§D2).
- **Host text**: the literal `value_to_text(default)` output [probe]. Twins are shown where they differ.
- **Class**: `paramclass_check` output [probe], or the engine-block/routing rule at `src/hypersaw_clap.cpp:1600-1640` [code].
- **PROPOSED tokens**:
  - **T** = the one time rule (D4), **F** = the one frequency rule (D4), **DIV** = a named division (D3).
  - **CURVE** = the D1 law, whatever is ruled (it must reach 0 where a row needs "off"), **LOG** = log10 kept.
  - **keep** = range unchanged.

### 2.1 Envelopes

| address | id(s) | min / max / default | unit: name / host / pres / gui2 | taper today (where) | host text @default | sync | class | PROPOSED unit · display · taper · range — reason |
|---|---|---|---|---|---|---|---|---|
| osc{1,2}.attack | 19, 1019 | 0.001 / 2 / 0.003 s | s / ms·s (19 only) / — / (s) + proxy [s] | log10 (pres → gen `gui2:1728`; hand proxy `:1287,:1302`) | `3.0 ms`; 1019 `0.003` | — | morphable | s · T · CURVE · 0–2 s (floor per D1) — the envelope complaint; 7 of the bank's 82 attack values sit on the 1 ms floor |
| osc{1,2}.decay | 20, 1020 | 0.005 / 4 / 0.16 | s / s (20 only) / — / (s) + [s] | log10 (same) | `0.16 s`; 1020 `0.160` | — | morphable | s · T · CURVE · keep |
| osc{1,2}.sustain | 21, 1021 | 0 / 1 / 1 | — / — / — / — | lin | `1.000` | — | morphable | level · keep (not a time) |
| osc{1,2}.release | 22, 1022 | 0.005 / 8 / 0.16 | s / s (22 only) / — / (s) + [s] | log10 | `0.16 s`; 1022 `0.160` | — | morphable | s · T · CURVE · keep |
| osc{1,2}.sAttack (S.envelope) | 65, 1065 | 0.001 / 2 / 0.004 | s / — / — / n/c | log10 in legacy `gui.html:313` only; none in pres | `0.004` | — | morphable | as ENV 1 attack — SPECTRA is parked; lowest priority |
| osc{1,2}.sDecay | 66, 1066 | 0.005 / 4 / 0.18 | s / — / — / n/c | legacy only | `0.180` | — | morphable | as ENV 1 decay |
| osc{1,2}.sSustain | 67, 1067 | 0 / 1 / 1 | — | — | `1.000` | — | morphable | level · keep |
| osc{1,2}.sRelease | 68, 1068 | 0.005 / 8 / 0.18 | s / — / — / n/c | legacy only | `0.180` | — | morphable | as ENV 1 release |
| penvA (ENV 2) | 162 | 0.001 / 2 / 0.003 | s / — / s / [s] | **lin** (gen `:1758`, proxy `:1314`) | `0.003` | — | device | s · T · CURVE · keep (already = ENV 1) |
| penvD | 163 | 0.005 / 4 / 0.16 | s / — / s / [s] | **lin** | `0.160` | — | device | as ENV 1 decay |
| penvS | 164 | 0 / 1 / 0 | — | lin | `0.000` | — | device | level · keep |
| penvR | 165 | 0.005 / 8 / 0.16 | s / — / s / [s] | **lin** | `0.160` | — | device | as ENV 1 release |
| env3A, env4A | 281, 285 | 0.001 / 2 / 0.003 | s / — / s / [s] | **lin** (`gui2:1322`, …) | `0.003` | — | device | as ENV 1 attack |
| env3D, env4D | 282, 286 | 0.005 / 4 / 0.16 | s / — / s / [s] | **lin** | `0.160` | — | device | as ENV 1 decay |
| env3S, env4S | 283, 287 | 0 / 1 / 0 | — | lin | `0.000` | — | device | level · keep |
| env3R, env4R | 284, 288 | 0.005 / 8 / 0.16 | s / — / s / [s] | **lin** | `0.160` | — | device | as ENV 1 release |
| sub.attack | 4012 | 0.0005 / 0.5 / 0.005 | — / — / s+log10 / [s] | log10 (`gui2:1776`) | `0.005` | — | morphable (engine rule) | s · T · CURVE · keep — **arrival time, not τ** (headline 13) |
| sub.release | 4013 | 0.002 / 2 / 0.08 | — / — / s+log10 / [s] | log10 (`:1777`) | `0.080` | — | morphable | s · T · CURVE · keep |

### 2.2 Glides, lags and other times

| address | id(s) | min / max / default | unit: name / host / pres / gui2 | taper today | host text | sync | class | PROPOSED |
|---|---|---|---|---|---|---|---|---|
| glide (Note Lag) | 33 | 0 / 2 / 0 s | s / off·ms·s / s+**log10** / [s] | **lin** — the table says log10 and the generator drops it because min = 0 (`gen_gui_controls.py:289`) | `off` | — | morphable | s · T with `off` at 0 · CURVE (must reach 0) · keep |
| sub.glide | 4018 | 0 / 2 / 0 | s / — / s / [s] | lin (`gui2:1781`) | `0.000` | — | morphable | as glide (range matched to 33 by design, `:1296-1297`) |
| freqGlide | 75 | 0 / 0.1 / 0 | s / — / — / (s) | lin | `0.000` | — | morphable | s · T (reads ms throughout) · CURVE · keep |
| morphGlide | 158 | 0 / 5 / 0.008 | s / — / s+**log10** / [s] | **lin** (same generator fallback) | `0.008` | — | device | s · T · CURVE (0 = instant) · keep |
| osc{1,2}.dissolve | 8, 1008 | 0.05 / 7.94 / 0.63 | s / s (8 only) / —+log10 / (s) | log10 (`gui2:1795`) | `0.63 s`; 1008 `0.630` | — | morphable | s · T · LOG (no "off" needed) · keep |
| osc{1,2}.onsetScatter | 91, 1091 | 0 / 80 / 0 **ms** | ms / — / —+**log10** / (ms) | **lin** (generator fallback) | `0.000` | — | morphable | ms-stored · T with `off` at 0 · CURVE · keep |
| bendTime | 107 | 5 / 1500 / 120 **ms** | ms / — / ms+log10 / [ms] | log10 | `120.000` | — | morphable | ms-stored · T · LOG · keep — the T rule must accept ms-stored and s-stored rows |
| noteTime | 139 | 5 / 1500 / 120 ms | ms / — / ms+log10 / [ms] | log10 | `120.000` | — | morphable | as bendTime |
| bendTau (Bend Lag) | 109 | 1 / 2000 / 60 ms | ms / — / ms+log10 / [ms] | log10 | `60.000` | — | morphable | as bendTime |
| fxXfadeMs | 265 | 5 / 500 / 80 ms | (dev) / — / ms / n/c (buried, ADR-163 A2) | — | `80.000` | — | device | ms · T · keep — buried; lowest priority |
| bendRate, noteRate | 108, 140 | 0.5 / 200 / 24 | st/s / — / st/s+log10 / [st/s] | log10 | `24.000` | — | morphable | st/s · `24.0 st/s` · LOG · keep |

### 2.3 LFO rates and beats

| address | id(s) | min / max / default | unit: name / host / pres / gui2 | taper today | host text | sync partner | class | PROPOSED |
|---|---|---|---|---|---|---|---|---|
| lfo{1,2}Rate | 269, 275 | 0.02 / 40 / 1 Hz | Hz / — / Hz+log10 / [Hz] | log10 (`gui2:1336,:1345`) | `1.000` | lfoNSync 271/277: `free (Hz)` / `tempo sync` (stepped) | device | Hz · F · LOG · keep |
| lfo{1,2}Beats | 272, 278 | 0.0625 / 8 / 1 | — / — / **`/beat` (wrong: beats per cycle)** / [/beat] | lin, step 0.005 (`gui2:1339`) | `1.000` | lfoNSync 271/277 | device | beats per cycle (stored) · **DIV** · stepped list per D3 · keep (1/64 note … 2 bars) — the human's "quantized to sensible divisions" |
| lfo{1,2}Phase | 274, 280 | 0 / 1 / 0 | — | lin | `0.000` | — | device | cycle fraction · display ° · keep — low stakes |

### 2.4 Delay times and beats

| address | id(s) | min / max / default | unit: name / host / pres / gui2 | taper today | host text | sync partner | class | PROPOSED |
|---|---|---|---|---|---|---|---|---|
| d{1..4}time | 232, 240, 248, 256 | 1 / 2000 / 375 ms | ms / — / ms+log10 / [ms] | log10 (`gui2:1539…`) | `375.000` | dNsync 233/241/249/257: `free (ms)` / `tempo sync` | morphable | ms-stored · T · LOG · keep |
| d{1..4}beats | 234, 242, 250, 258 | 0.0625 / 8 / 0.5 | — / — / **`/beat` (wrong: beats per repeat)** / [/beat] | lin, step 0.005 (`gui2:1541…`) | `0.500` | dNsync | **morphable** (so a blend-morph can produce off-list values) | beats per repeat · **DIV** · stepped list per D3 · keep — the delay law is `beats × 60000/bpm` (`delay_core.h:229-230`) |
| d{1..4}offR | 235, 243, 251, 259 | 0.25 / 2 / 1 | — / — / x / [x] | lin | `1.000` | — | morphable | ratio · `×1.00` · LOG (×0.5 and ×2 equidistant from ×1; ×1 sits at 43 % of linear travel today) · keep |
| d{1..4}hp | 239, 247, 255, 263 | 0 / 500 / 60 Hz | Hz / — / Hz / [Hz] | lin | `60.000` | — | morphable | Hz · F with `off` at 0 · CURVE · keep — 0 = OFF and 0<x<5 is clamped to 5 Hz (`delay_core.h:133-137`), so today's first 1 % of travel is dead |

### 2.5 Filter and crossover frequencies, and other rates

| address | id(s) | min / max / default | unit: name / host / pres / gui2 | taper today | host text | sync partner | class | PROPOSED |
|---|---|---|---|---|---|---|---|---|
| bassMonoHz | 41 | 60 / 500 / 120 Hz | Hz / — / — / (Hz) | lin (`gui2:1212`) | `120.000` | — | morphable | Hz · F · LOG · keep |
| sub.tone | 4010 | 30 / 20000 / 20000 Hz | — / — / Hz+log10 / [Hz] | log10 (`:1775`) | `20000.000` | — | morphable | Hz · F (`20.0 kHz`) · LOG · keep |
| bendSpringF, noteSpringF | 110, 141 | 0.5 / 20 / 4 Hz | Hz / — / Hz+log10 / [Hz] | log10 | `4.000` | — | morphable | Hz · F · LOG · keep |
| bendQTimeHz (Step Rate) | 147 | 0.2 / 50 / 8 Hz | Hz / — / Hz+log10 / [Hz] | log10 | `8.000` | bendQTimeMode 146: `continuous` / `free (Hz)` / `sync` | morphable | Hz · F · LOG · keep |
| osc{1,2}.driftRate | 10, 1010 | 0 / 1 / 0.4 (knob; shell maps to 0.2–8.2 /s) | — / `/s` (10 only) / — / — | lin knob domain | `3.4 /s`; 1010 `0.400` | — | morphable | knob · `/s` on both oscs · keep — already the §D2 "tapered knob" idiom |

### 2.6 Tempo grids (cycles *per* beat, the opposite convention to 2.3/2.4)

| address | id(s) | min / max / default | unit: name / host / pres / gui2 | taper today | host text | sync partner | class | PROPOSED |
|---|---|---|---|---|---|---|---|---|
| osc{1,2}.beatMult (Grid Cycles/Beat) | 23, 1023 | 0.25 / 8 / 1 | Cycles/Beat / name/beat (23 only) / — (widget `select`) / — | gui2 renders a **linear slider** (`gui2:1807`, step 0.005) although the table says `select`; snapped to `kGridSteps` for **23 only** (`:6680`) | `1/beat`; 1023 `1.000`; 23 at 1/3 prints `1/3/beat` | Detune Law (5) = `tempo-grid` (3) | morphable | cycles per beat · named step · stepped list `kGridSteps` · keep — fix the osc-2 snap and name collision (headline 7–8) |
| bendQTimeSync (Step Grid) | 148 | 0.25 / 8 / 4 | — / — / /beat / [/beat] | lin; snapped (`:7052`) | `4.000` | bendQTimeMode 146 | morphable | steps per beat · named step · stepped list · keep |

### 2.7 Pitch and cents

| address | id(s) | min / max / default | unit: name / host / pres / gui2 | taper | host text | class | PROPOSED |
|---|---|---|---|---|---|---|---|
| osc{1,2}.fineCents | 37, 1037 | −100 / 100 / 0 | c / c / — / (c); mixer proxy `—` (`gui2:1165`) | lin | `+0.0 c` (both — baseIdOf) | morphable | c · `+0.0 c` · LIN · keep |
| gFine | 102 | −100 / 100 / 0 | — / — / — / — (`gui2:1205`) | lin | `0.000` | morphable | c — master fine prints bare while osc fine prints cents |
| pitchBend | 38 | −12 / 12 / 0 | — / st / — / hand-coded `' st'` (`gui2:2815,:2825`) | lin | `+0.00 st` | morphable | st · keep |
| osc{1,2}.oscPitch | 181, 1181 | −24 / 24 / 0 | (cont.) / — / st / (st) | lin | `0.000` | morphable | st · `+0.00 st` · keep |
| modEnvPitch | 161 | −48 / 48 / 0 | — / — / st / [st] | lin | `0.000` | device | st · keep |
| sub.fine | 4006 | −100 / 100 / 0 | — / — / c / [c] | lin | `0.000` | morphable | c · keep |
| sub.pitchMod | 4019 | −48 / 48 / 0 | st / — / st / [st] | lin | `0.000` | morphable | st · keep |
| osc{1,2}.driftDepth | 9, 1009 | 0 / 100 / 0 | c / c (9 only) / — / (c) | lin | `0.0 c`; 1009 `0.000` | morphable | c · keep |
| osc{1,2}.basin | 30, 1030 | 10 / 50 / 35 | c / — / — / (c) | lin | `35.000` | morphable | c · keep |
| bendHyst, noteHyst | 115, 145 | 0 / 50 / 8 | c / — / c / [c] | lin | `8.000` | morphable | c · keep |

### 2.8 Everything else that has a unit

| address | id(s) | min / max / default | unit: name / host / pres / gui2 | host text | class | PROPOSED |
|---|---|---|---|---|---|---|
| osc{1,2}.alpha (Phase Lag) | 27, 1027 | −90 / 90 / 0 | — / deg (27 only) / — / — | `+0 deg`; 1027 `0.000` | morphable | deg · `+0°` · keep — no unit anywhere in gui2 |
| voiceCull | 160 | −80 / −40 / −80 | — / — / dB / [dB] | `-80.000` | structural | dB · keep |
| sub.bumpPhase | 4003 | −π / π / −0.25 | — / — / rad / [rad] | `-0.250` | morphable | rad (or °) · keep |
| inertiaCurve | 70 | 0.3 / 5 / 2.5 | (dev) / — / — / (dev) | `2.500` | morphable | exponent · keep (dev) |

### 2.9 Unitless continuous controls (no unit proposed; listed for completeness)

Each of these reaches the host as bare `%.3f` [probe], carries no unit in any of the three places, and is linear everywhere unless noted. PROPOSED for all: **keep; no unit**. Showing them as % or dB is a separate question, outside B213's named scope (§4).

| group | id(s) | range(s) | class |
|---|---|---|---|
| swarm per-osc (each + 1000 twin) | 4, 6, 7, 12, 13, 14, 16, 17, 26, 29, 39, 42, 56, 69, 71, 72, 76, 78, 79, 80, 81, 82, 85, 92, 93, 95, 129, 130, 131, 132 and 1004, 1006, 1007, 1012, 1013, 1014, 1016, 1017, 1026, 1029, 1039, 1042, 1056, 1069, 1071, 1072, 1076, 1078, 1079, 1080, 1081, 1082, 1085, 1092, 1093, 1095, 1129, 1130, 1131, 1132 | 0–1 mostly; K/onset/rtone/wtilt/toneTilt/roundHi −1–1; normExp 0.5–1; width 0–1.5; harmReach 0.25–4; stretchB 0–6; spread 1–24; onsetAlpha 0–1.5 | morphable |
| SPECTRA per-osc (no gui2 control) | 45, 46, 48, 49, 51, 53, 54 and 1045, 1046, 1048, 1049, 1051, 1053, 1054 | 0–1; tilt 0.5–2; wtilt −1–1 | morphable |
| inertia (global, knob domain, core gets √ or pow, `:6685-6691`) | 11 | 0–1 | morphable |
| FX slots amt / tone ("Resonance" in pres) / mix | 58, 60, 62, 64; 96, 97, 98, 99; 133, 134, 135, 136 | 0–1 | morphable |
| FX slot size/spread/damp/noise/stereo | 200, 201, 203, 204, 205, 208, 209, 211, 212, 213, 216, 217, 219, 220, 221, 224, 225, 227, 228, 229 | 0–1 | morphable |
| delay fb / cross / damp | 236, 237, 238, 244, 245, 246, 252, 253, 254, 260, 261, 262 | 0–1 | morphable |
| bend/note law shape | 111, 112, 113, 142, 143 | damp 0–1; distOver 0–2; return 0.2–3 (label "Return x") | morphable |
| morph surface | 152, 153, 154 (log10 in gui2), 155 | 0–1; temp 0.02–4 | device |
| master volume | 100 | 0–1.5 | device |
| macros | 166, 167, 168, 169, 170, 171, 172, 173 | 0–1 | device |
| sub | 4001, 4002, 4007, 4008 | width 0.05–0.95; bumpAmt 0–0.6; level 0–1; phase 0–1 | morphable |
| routing crosspoints (ADR-088) | 10000–10003, 10064–10067, 10128–10131, 10513–10515, 10578, 10579, 10643 | −2–2 | morphable (`:1602-1607`) |
| routing out / init / src-out | 20000–20003; 21000–21003; 22000–22002 | 0–2; −1–1; 0–2 | morphable |

**Count check.** §2.1–2.8 list 96 ids and §2.9 lists 170, for 266 in all. That equals the 266 non-stepped ids the probe enumerated (Appendix B).

---

## D1 — The taper law

**Facts.**
- There are 30 `log10` rows today. Log10 cannot reach 0, so the generator (`gen_gui_controls.py:289`) silently emits **linear** for the three log10 rows whose min is 0 (33, 158, 91/1091). Every log10 row with min > 0 has a hard floor, e.g. attack 1 ms.
- The cores accept 0 for the one-pole envelopes: `onePoleCoef(0)` = 1 − e^(−∞) = 1, an instant step (`force_core.h:61-64`). ENV 2–4 clamp τ at 0.1 ms (`hypersaw_clap.cpp:2801`).
- The bank uses the floor. 7 of 82 factory attack values are exactly 0.001 s, and 74 of 82 are ≤ 6 ms.
- gui2 drags 200 px for full travel (`gui2.html` `wireKnob`), so 1 px = 0.5 % of travel.

**Measured: fraction of control travel below 10 ms / 100 ms / 1 s.** This is arithmetic over the declared ranges (script in Appendix A). "k" is a power curve `v = max · t^k`, which reaches 0.

| parameter | range | gui2 today | host lane (linear) | log10 | power k=2 | k=3 | k=4 |
|---|---|---|---|---|---|---|---|
| attack (19), S.A (65), ENV2–4 A, bendTau, dNtime | 0.001–2 s | log10 (19, 109, dN); **lin** (ENV2–4) | 0.5 / 5.0 / 50.0 % | 30.3 / 60.6 / 90.9 % | 7.1 / 22.4 / 70.7 % | 17.1 / 36.8 / 79.4 % | 26.6 / 47.3 / 84.1 % |
| decay (20, 66, ENV2–4 D) | 0.005–4 s | log10 / **lin** | 0.1 / 2.4 / 24.9 % | 10.4 / 44.8 / 79.3 % | 5.0 / 15.8 / 50.0 % | 13.6 / 29.2 / 63.0 % | 22.4 / 39.8 / 70.7 % |
| release (22, 68, ENV2–4 R) | 0.005–8 s | log10 / **lin** | 0.1 / 1.2 / 12.4 % | 9.4 / 40.6 / 71.8 % | 3.5 / 11.2 / 35.4 % | 10.8 / 23.2 / 50.0 % | 18.8 / 33.4 / 59.5 % |
| sub.attack (4012) | 0.0005–0.5 s | log10 | 1.9 / 19.9 / 100 % | 43.4 / 76.7 / 100 % | 14.1 / 44.7 / 100 % | 27.1 / 58.5 / 100 % | 37.6 / 66.9 / 100 % |
| sub.release (4013) | 0.002–2 s | log10 | 0.4 / 4.9 / 49.9 % | 23.3 / 56.6 / 90.0 % | 7.1 / 22.4 / 70.7 % | 17.1 / 36.8 / 79.4 % | 26.6 / 47.3 / 84.1 % |
| glide (33), sub.glide (4018) | 0–2 s | **lin** | 0.5 / 5.0 / 50.0 % | — (cannot) | 7.1 / 22.4 / 70.7 % | 17.1 / 36.8 / 79.4 % | 26.6 / 47.3 / 84.1 % |
| morphGlide (158) | 0–5 s | **lin** | 0.2 / 2.0 / 20.0 % | — | 4.5 / 14.1 / 44.7 % | 12.6 / 27.1 / 58.5 % | 21.1 / 37.6 / 66.9 % |
| dissolve (8) | 0.05–7.94 s | log10 | 0 / 0.6 / 12.0 % | 0 / 13.7 / 59.1 % | 3.5 / 11.2 / 35.5 % | 10.8 / 23.3 / 50.1 % | 18.8 / 33.5 / 59.6 % |
| bendTime / noteTime | 5–1500 ms | log10 | 0.3 / 6.4 / 66.6 % | 12.2 / 52.5 / 92.9 % | 8.2 / 25.8 / 81.6 % | 18.8 / 40.5 / 87.4 % | 28.6 / 50.8 / 90.4 % |
| onsetScatter (91) | 0–80 ms | **lin** | 12.5 / 100 / 100 % | — | 35.4 / 100 / 100 % | 50.0 / 100 / 100 % | 59.5 / 100 / 100 % |
| freqGlide (75) | 0–100 ms | lin | 10.0 / 100 / 100 % | — | 31.6 / 100 / 100 % | 46.4 / 100 / 100 % | 56.2 / 100 / 100 % |

**What a power curve reaching zero spends below the old floor** (0 → old min): attack 7.9 % (k=3) or 15.0 % (k=4); decay 10.8 / 18.8 %; release 8.5 / 15.8 %; dissolve 18.5 / 28.2 %; bendTime 14.9 / 24.0 %.

**Options and their costs.**
- **(a) log10 everywhere (today), keeping a floor.**
  - *For:* it is scale-invariant, so a drag of N px is the same *ratio* anywhere. It gives the most travel to the smallest times (30 % of an attack below 10 ms).
  - *Against:* it cannot reach 0. It forces the generator's silent linear fallback on three rows, and "0 ms" can never be dialled.
- **(b) one power/skew law for all times.**
  - *For:* it reaches 0, and it bends less at the top. For k = 3 the attack keeps 17 % of travel below 10 ms against today's 30 %.
  - *Against:* it is not ratio-uniform, so the D6 "curved space" is no longer a constant ratio. With k = 3, 8–19 % of travel sits below today's floor.
- **(c) log10 for frequencies and rates, skew for times.**
  - *For:* this is the ROADMAP's split. Frequencies need no zero: every Hz row above has min > 0, and d*hp's 0 = off is the one exception.
  - *Against:* it makes two laws, and D6 has to be answered for each.
- **(d) log10 plus an explicit zero detent**, i.e. the bottom few percent reads `off`/`0`.
  - *For:* it keeps log's ratio behaviour and reaches 0. There is precedent: the host text for glide already prints `off` below 1 ms (`:8452-8456`), and `gui.html:526,535-536` does the same for glide, freqGlide and onsetScatter.
  - *Against:* it adds a discontinuity, and a morph or mod passing through the detent jumps.

---

## D2 — Host automation lanes

**How each format presents the range [code].**
- **CLAP**:
  - `clap_param_info` carries `min_value`, `max_value`, `default_value` and flags. Flags run from `IS_STEPPED` to `IS_ENUM` (`libs/clap/include/clap/ext/params.h:136-206`). **There is no taper, skew or display-curve field in CLAP 1.2.10.** A CLAP host has plain values and a `value_to_text` callback to work with, and nothing else.
  - The plugin reports plain min/max (`hypersaw_clap.cpp:8410-8411`).
- **VST3 (clap-wrapper v0.15.1)**:
  - The value is normalised by `asVst3Value = (v − min)/(max − min)`, and `asClapValue = x·(max − min) + min` (`libs/clap-wrapper/src/detail/vst3/parameter.h:60-74`). That is linear, with no hook.
  - Host automation arriving as normalised values is converted at `src/detail/vst3/process.cpp:371`.
  - Display goes through our `value_to_text` (`src/wrapasvst3.cpp:505-544`). Typed entry goes through our `text_to_value` (`:546-566`).
- **AUv2 (same wrapper)**:
  - `minValue` and `maxValue` are passed as-is (`src/wrapasauv2.cpp:527-528`). The flags built at `src/detail/auv2/parameter.cpp:24-66` set no `kAudioUnitParameterFlag_Display*`; the macOS SDK defines `DisplayLogarithmic` as 1<<22 and nothing in the wrapper emits it. So the host sees a linear parameter.
  - Display goes through `value_to_text` (`wrapasauv2.cpp:719-733`). **There is no `ParameterValueFromString` handler**, so typed entry in an AU host never reaches `text_to_value`.
  - Aside, not ours: the wrapper ORs `kAudioUnitParameterUnit_Indexed`/`_Boolean` (unit enum values 1/2) into the *flags* word (`parameter.cpp:36-42`), where bits 0–1 are not defined flags.

**What drawing an attack lane gives today (VST3/AU, and CLAP if the host draws plain values linearly)** [entailed]:

| lane height | attack |
|---|---|
| 0.1 % | 3.0 ms (the default) |
| 0.5 % | 11.0 ms |
| 1 % | 21.0 ms |
| 10 % | 200.9 ms |
| 50 % | 1000.5 ms |

The whole 1–10 ms decade, where 74 of 82 factory attacks live, is the bottom 0.45 % of the lane. For comparison, the same decade is 30 % of gui2's knob travel.

**Options.**
- **(a) Keep honest units, accept poor lanes.**
  - *Cost:* zero migration.
  - *Needed anyway:* fix the host text (§3), so a lane at least *reads* right.
- **(b) Expose a tapered 0..1 parameter.**
  - *Precedent:* the idiom already exists in this shell. Drift Rate (10) is a 0..1 knob that the host text renders as `/s` (`:8487-8490`). Inertia (11) is a 0..1 knob whose core value is √ or pow (`:6685-6691`, readback `:7241`).
  - **Cost, re-using the same ids:**
    - Every stored preset and state value is in seconds, so it would need a versioned conversion. That is doable: the chunk is versioned and JSON presets carry `schema`.
    - **Host automation cannot be migrated by us.** A CLAP lane stores plain values, so an old `0.5` (s) would be read as knob 0.5. A VST3/AU lane stores normalised values, so an old x would be re-read through the new curve.
    - The plugin id is frozen (`:63-65`).
  - **Cost, with appended new ids** (old ids kept, `IS_HIDDEN`, following the retired-id idiom of 4011 and 174–177): there would be two ids per time parameter. Morph, preset keys, routing destinations and gui_reach would all need to know which one is live.
- **(c) Get a curve into the lane without changing the parameter.** This is not available with the pinned stack. CLAP has no field for it, VST3 normalisation is linear in the wrapper, and AU `DisplayLogarithmic` would need a patch to the wrapper submodule. That patch is outside this repo's tree and would be a dependency change.

**Could not determine:** how any particular host draws a CLAP lane (plain values linear, or its own curve). Ableton was not run, and no host log was read.

---

## D3 — Tempo divisions

**Every place a continuous "beats" value is stored today:**

| id(s) | meaning | quantised today? | host text | gui2 control |
|---|---|---|---|---|
| 23 / 1023 beatMult | **cycles per beat** (tempo-grid detune law) | 23 snapped to `kGridSteps` at `applyParam` (`:6680`); **1023 not** [probe: 0.9 → 0.9] | 23 named (`1/beat`), 1023 bare | linear slider, step 0.005 |
| 148 bendQTimeSync | **steps per beat** (`resolveQTimeMs`, `:2598-2607`) | snapped (`:7052`) [probe: 0.9 → 1] | bare `4.000` | linear, `/beat` |
| 234 / 242 / 250 / 258 dNbeats | **beats per repeat** (`delay_core.h:229-230`) | no [probe: 0.9 → 0.9] | bare | linear, labelled `/beat` (wrong) |
| 272 / 278 lfoNBeats | **beats per cycle** (`freq = (bpm/60)/beats`, `:3822`) | no [probe: 0.9 → 0.9] | bare | linear, labelled `/beat` (wrong) |

Also in code: `DelayCore::Params::timeBeats` (`delay_core.h:59`), `Plugin::lfoBeats[]` (`:2833`), `kGridSteps` / `kGridStepNames` (`:1661-1664`).

**What the factory bank holds** [probe over 41 patches + 4 corner files; 7 state fixtures grepped]:

| where | parameter | value | sync | on B208's list? | on `kGridSteps`? |
|---|---|---|---|---|---|
| FX - Delay Dotted | d1beats | 0.75 | d1sync = 1, fx1type = 9 (delay) | yes (3/16 = 1/8D) | n/a |
| MISC - Tempo Grid Lattice | osc 1 beatMult | 2 (corners hold 1; morph off) | law = tempo-grid | n/a | yes |
| every other patch, corner file and fixture | all beats ids | defaults (0.5 / 1 / 4) | off | yes | yes |

No factory patch turns on LFO sync. **A snap to nearest therefore changes zero factory patches and zero fixtures**, so `statefix_check`'s bit-identical load is unaffected for these ids [entailed].

**The proposed list.** B208 (branch `lab-modulator`, `015b612`, not yet merged) proposes 22 divisions inside the shipped 0.0625–8 range:
- straight 2/1 … 1/64;
- dotted 1/1D … 1/64D;
- triplet 2/1T … 1/32T.

Its worst adjacent ratio is 4:3, so an off-list value snaps by at most **±15.47 %**. Its trace also records nearest(0.75) = 3/16 and nearest(0.3) = 1/8T. This row adopts that list and does not write a second one. **The row's "1/64 … 8 bars" goes beyond the current range**: 8 bars = 32 beats against max 8. Reaching it is a D5 widen, with the VST3/AU cost in §D5.

**What a snap does to each:**
- **lfoNBeats** is device class and not a modulation destination (`:750-751`, `:3437`). A snap is cosmetic plus its effect on user patches.
- **dNbeats** is **morphable and a legal mod destination** (`modAddRoute`, `:3423-3438`, refuses only stepped ids, 161–177 and 269–288). With a snap at `applyParam`, a blend-morph or an LFO route onto delay beats would **step** between divisions instead of sweeping. That may be desirable (no pitch-bend zipper on a delay) or not. Either way it is part of D6.
- **beatMult / 148** use the opposite (per-beat) convention, and `kGridStepNames` names 0.25 `"1/4"`. A single division vocabulary across all four would give "1/4" two opposite meanings unless the grid list is renamed (e.g. `4 beats`, or `1 per bar`).

**Options.**
- **(a) Stepped index**, i.e. a new meaning for the same id.
  - *For:* automation lands only on names.
  - *Against:* it breaks every stored value and every existing host lane (frozen ids), unless it is a new id.
- **(b) Continuous beats kept; snap in `applyParam` plus named display and entry.** This is the id-23 pattern already in the shell, and B208's recommendation.
  - *For:* zero migration for on-list values. An off-list user value snaps to nearest on load. The snap should be recorded, e.g. an undo/history note or a log line.
  - *Against:* host lanes remain continuous. A drawn ramp plays as a staircase and reads as names.
- **(c) Display-only snap**, no `applyParam` change.
  - *For:* it is the cheapest.
  - *Against:* the sound and the name disagree for off-list values, which is the B177 "shape you see ≠ shape you hear" failure again.

---

## D4 — One ms/s rule everywhere

**The rules that exist today** [code]:

| where | rule | example outputs |
|---|---|---|
| host, ids 19/20/22 (osc 1 only) | `< 0.01 s → "%.1f ms"`, else `"%.2f s"` (`:8447-8451`) | 0.003 → `3.0 ms`; 0.0095 → `9.5 ms`; **0.010, 0.012, 0.014 → all `0.01 s`**; 0.05 → `0.05 s`; 0.25 → `0.25 s` |
| host, glide 33 | same, plus `< 0.001 → "off"` (`:8452-8457`) | 0 → `off` |
| host, dissolve 8 | `"%.2f s"` always (`:8443-8446`) | 0.63 → `0.63 s` |
| host, everything else | `"%.3f"` | 375 ms delay → `375.000`; 20 kHz tone → `20000.000` |
| gui2 readout | `toFixed(2)`, unitless (`gui2.html:2321`) | 0.003 → `0.00`; 0.006 → `0.01`; 375 → `375.00` |
| gui2 `fmtHz` (spectrum hover only) | `≥ 1000 → kHz` (`gui2.html:5815-5816`) | 2000 → `2.00 kHz` |
| legacy gui.html | the host's `< 0.01` rule for every `data-log10` control (`:522`); glide/freqGlide/onsetScatter by id (`:526,:535-536`) | 0.004 → `4.0ms` |

So there are three time rules (host, legacy GUI, none in gui2), and one Hz→kHz rule that serves only the spectrum.

**The candidate rule from the row** is: ms under 1 s, s at 1 s and above, and kHz at 1 kHz and above. Applied identically in the host text and gui2, it gives:
- 0.003 → `3.00 ms`
- 0.0125 → `12.5 ms`
- 0.25 → `250 ms`
- 1.2 → `1.20 s`
- 375 (ms-stored) → `375 ms`
- 20000 → `20.0 kHz`

That example uses 3 significant figures. The precision itself is part of the ruling. **Two inputs the rule must take:**
- **Storage unit.** Five families store ms (107, 109, 139, 91, 232…, 265), and the rest store seconds. The rule should format the *quantity*, so the unit column must say which one is stored.
- **What the number means.** One-pole τ (ENV 1, S.env, ENV 2–4) and arrival time (sub) print identically today (headline 13). B208 proposes arrival time throughout. If that lands, the displayed number for an old patch changes by ×4.61 (τ × ln 100) for the same sound. Ruling D4 before B208's law is settled risks re-labelling twice.

**Typed entry is part of D4.** Today the host's own text does not parse back (headline 2). Phase 3 needs `text_to_value` to accept what `value_to_text` prints (`3.00 ms`, `1.20 s`, `2.00 kHz`, `1/8T`). It also needs the `minV + index` fix for labelled rows (Poles q). AU never calls it (§D2).

---

## D5 — Ranges

**Facts** [probe]:
- The ADSR ranges are **already identical** across ENV 1, the S.envelope, ENV 2, ENV 3 and ENV 4. The sub differs (A 0.0005–0.5, R 0.002–2, no D/S). glide and sub.glide match (0–2).
- The factory bank's time usage runs from attack 0.001 to 1.5 s, decay 0.09–3 s and release 0.07–3 s. dissolve reaches 0.08–7 s, **7 of its ceiling of 7.94**. morphGlide runs 0.008–1.5 s. Nothing exceeds a current max. The attack floor is used 7 times.
- `applyParam` clamps every write to [min, max] (`:6679`), so **narrowing** a range clamps stored values silently. That includes state load, preset load, morph and host.
- **Widening** keeps every *stored* value valid, because state and presets store plain units.

**The cost the row did not count** [entailed from `parameter.h:60-66` + `process.cpp:371`]:
- VST3 and AU hosts address automation in normalised units, and the wrapper maps them through the current min and max. A changed min or max therefore re-maps every existing VST3/AU automation point for that parameter. Example: widening release 0.005–8 → 0.005–20 moves an old 50 % point from 4.0 s to 10.0 s.
- CLAP hosts store plain values, so for CLAP a widen is safe.
- Moving the attack floor from 0.001 to 0 (D1 option b) is also a min change, and so also a VST3/AU re-map, though a small one: an old point at x moves by 0.001·(1−x) s.

**Options.**
- (a) Keep all ranges and fix only taper and text.
- (b) Widen selected maximums (the row's "8 bars" for beats; longer releases), accepting the VST3/AU automation re-map.
- (c) Widen only through new ids, which carries §D2(b)'s cost.

---

## D6 — Morph and modulation in curved space

**Where the linear-units assumption lives today** [code]:

| mechanism | law | where |
|---|---|---|
| morph **blend** mode (`morphMode` 1) | bilinear weighted sum of RAW corner values | `hypersaw_clap.cpp:4088-4092` |
| morph **quantum** mode (0, default) | picks a corner (no interpolation), then a one-pole in RAW units at `morphGlide` τ | `:4094-4114`, `morphApplyTarget :3961-3967` |
| mod matrix, generic destination | `base + Σ(depth·src)·(max − min)`, clamped | `:3881-3891` |
| macro capture (bakes the macro offset into a corner) | same linear span | `:3309-3319` |
| intent bus (flag ships OFF) | normalises by linear span | `:4236-4245` |
| gui2 live halo | depth is taken in VALUE space and mapped through the control's own curve, **consistent with the engine** | `gui2.html:3461-3478` |
| gui2 `setKnobMod` (ADR-121 seam, **no caller**) | depth as a fraction of the CONTROL's range, **inconsistent with the engine on log controls** (latent) | `gui2.html:2349-2386` |

**Scope of the modulation half.** `modAddRoute` refuses 161–177 and 269–288 (`:3427-3437`). So ENV 2–4's own times are not mod destinations. The modulation half of D6 reaches ENV 1, the S.envelope, glides, dissolve, the bend/note times, the delay times and beats, and every frequency. The morph half reaches every morphable row in §2.

**Worked example 1: morph blend between corners holding attack 5 ms and 5 s.**

| weight toward 5 s | linear (today) | log / geometric |
|---|---|---|
| 0.10 | 504.5 ms | 10.0 ms |
| 0.25 | 1.254 s | 28.1 ms |
| **0.50** | **2.503 s** | **158 ms** |
| 0.75 | 3.751 s | 889 ms |
| 0.90 | 4.501 s | 2.506 s |

Under linear blending, 1.9 % of the path lies below 100 ms and 20 % below 1 s. **60 % of the path is above 2 s**, which corrects the row's "almost the whole journey". Under log blending, 43 % of the path lies below 100 ms and 77 % below 1 s.

**Worked example 2: an LFO route onto attack** (0.001–2 s, base 5 ms, depth as a fraction of range, upward).

| depth | today (linear range) | in the control's log10 space |
|---|---|---|
| 0.01 | **25.0 ms** | 5.4 ms |
| 0.05 | 105 ms | 7.3 ms |
| 0.25 | 505 ms | 33.4 ms |

Today's law over-moves short times: depth 0.01, the smallest non-trivial amount, is a 5× change to a 5 ms attack. In control space, a given depth is the same *ratio* at any base (log10), or roughly so (power curve, §D1).

**Consequences to weigh:**
- A curved-space morph or mod changes how every existing blend-morph or route onto a time parameter **sounds**. The factory bank has none (headline 10), and the number in user sets is unknown.
- It also moves the delay-beats case into the D3 answer. A blend between 1/8 (0.5) and 1/4 (1) reads 0.75 (1/8D) at halfway linearly, but 0.707 geometrically, which B208's list snaps to 1/4T (0.667).
- Under a power-curve D1 ruling, "curved space" is no longer ratio-uniform, so D1 and D6 are coupled.
- The engine and the GUI must use one law. Today the live halo already maps value-space depth through the control curve, so the engine law and the halo agree. `setKnobMod` does not, and should be deleted or re-specified before anything calls it.

---

## 3. Every place a unit, format or taper rule is hard-coded by id

These are the lines Phase 3 would replace with generated ones. All are at `fb64f70`.

**`src/hypersaw_clap.cpp`, host text/entry:**
- `:8443` `id == 8` (dissolve s). Osc 1 only.
- `:8447` `id == 19 || id == 20 || id == 22` (envelope ms/s). Osc 1 only.
- `:8452` `id == 33` (glide off/ms/s).
- `:8458` `baseIdOf(id) == 35` (octave). **Dead: the stepped branch at `:8439` wins.**
- `:8462` `baseIdOf(id) == 36` (semitones). **Dead, same reason.**
- `:8466` `id == 27` (phase lag deg). Osc 1 only.
- `:8470` `baseIdOf(id) == 37` (fine c). Both oscs.
- `:8474` `id == 38` (pitch st).
- `:8478` `id == 23` (named grid step). Osc 1 only.
- `:8483` `id == 9` (drift depth c). Osc 1 only.
- `:8487` `id == 10` (drift rate `/s` from the knob). Osc 1 only.
- `:8508` `*out = i` (label entry ignores `minV`).
- `:8512` `atof(text)` (no unit parsing).

**`src/hypersaw_clap.cpp`, value domain:**
- `:6680` `id == 23` snap (osc 1 only).
- `:6685-6691` `id == 11` inertia taper.
- `:6692` `id == 70` taper exponent.
- `:7048-7052` `146..148`, the qTime snap.
- `:7241` `d->id == 11` knob-domain readback.

**`src/gui/gui2.html`:**
- `:2321` `displayValue`, the unitless `toFixed(2)` for every knob.
- `:2303` `fmtVal`, unitless (hint only).
- `:2316-2317` `ctlToParam` / `paramToCtl` (log10 only, no other law).
- `:2815`, `:2825` id 38 `' st'`.
- `:5815` `fmtHz` (spectrum only).
- `:6698` boot paints `displayValue(el, +el.value)`, the **control** value. A log10 knob therefore reads its log (attack `-2.52`) until the first shell echo repaints it [entailed; not observed].
- Hand-placed log10/linear copies outside `GEN:` blocks: `:1287-1290`, `:1302-1305` (ENV 1 proxies, log10), `:1314-1317` (ENV 2 proxy, linear).

**`src/gui/gui.html` (legacy, `HYPERSAW_GUI2=OFF`):** `:522-546`, a per-id list: 9, 10, 23, 33, 35, 36, 37, 38, 27, 30, 39, 42, 72, 78, 82, 75, 91, 93, 95, 76, 81, 41, 53, 55, 56, 54, 14.

**`tools/gen_gui_controls.py`:** `:289` `scale == "log10" and p["min"] > 0`, the silent linear fallback. `:265` copies `unit` into the label span.

**`src/param_presentation.tsv`:** the four `/beat` units on `lfo{1,2}Beats` and `d{1..4}beats` (wrong direction, headline 7). Units embedded in 24 labels.

---

## 4. What I could NOT determine

- **How any specific host draws a CLAP automation lane**, and whether Ableton (the human's host) loads the CLAP or the VST3/AU. No host was run.
- **Whether VST3/AU hosts also store their parameter *snapshots* normalised.** The automation path is code-evidenced; the snapshot path is not. Our own state chunk is plain units.
- **The gui2 boot flash** (a log value shown before the first echo) is read from code, not seen in a running GUI.
- **User patches.** Every "changes nothing" claim covers the factory bank (41 patches, 4 corner files) and the 7 state fixtures only. Automation already recorded in users' sets is unknowable from here.
- **Semantics of some unitless rows.** I did not read the cores for Octave Spread (1–24), Harmonic Reach, Stretch B or the FX slot 0–1 controls, so they are listed "no unit" by default, not because a unit was ruled out. The same goes for whether Master Volume/Volume are linear gain (a dB display question).
- **gui2 typed entry.** I did not establish whether gui2 offers typed value entry anywhere; I found no text-entry path for knobs in the lines read.
- **B208's list itself** is on an unmerged branch (`lab-modulator`). I read its trace, not the running lab.

---

## Appendix A — how to reproduce (scratch probes, not committed)

Build `HYPERSAW-impl` (Release, Unix Makefiles) into any scratch dir. Then link each probe against `libHYPERSAW-impl.a -framework WebKit -framework Cocoa`, with `-I libs/clap/include -I src`.

- **units_probe**: `create_plugin("com.lifted-truck.hypersaw")`, then for each index `params.get_info`, printing `value_to_text` at the default, the min and the max. It gives 397 params, 266 of them continuous.
- **snap_probe**: queues `CLAP_EVENT_PARAM_VALUE` 0.9 on 23, 1023, 148, 234 and 272 through `params.flush`, then reads `get_value`, followed by the `text_to_value` cases in headline 2.
- **corner_probe**: prints `hypersaw_debug_cornervals(p, 0)`, whose key order is the slot→id order of `morphCorners` in the factory JSON. It was used to read beats and time values out of the corners.
- `paramclass_check` (the shipped target), run standalone, for the class column.
- **taper arithmetic**: the fraction of travel below T is `(T−min)/(max−min)` for linear, `log(T/min)/log(max/min)` for log10 and `(T/max)^(1/k)` for power. The morph examples are `a + w(b−a)` against `a·(b/a)^w`.

## Appendix B — continuous-id enumeration

`units_probe` reports 397 parameters, of which 266 have `CLAP_PARAM_IS_STEPPED` clear. They are:
- **kParams, osc 1 and globals (4–288):** 172 ids.
- **Osc 2 twins (1004–1181):** 54 ids.
- **SUB OSC block (4001–4019):** 11 ids.
- **Routing (10000–22002):** 29 ids.

Each appears in exactly one row of §2.1–2.9.
