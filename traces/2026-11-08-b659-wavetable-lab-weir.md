# 2026-11-08 — B659: Weir's filter in the wavetable lab

> Origin: horde lead session, 2026-11-08, B659. The human, by poll: "Weir in the wavetable lab", to start testing filters.

**What changed.** `docs/wavetable/lab/wavetable-lab.html`; two new files, `docs/wavetable/lab/weir/weir_lab.wasm` (a
vendored binary of Weir's C++) and `docs/wavetable/lab/weir/PROVENANCE.md`; this trace. No spec, reference, ROADMAP,
DECISIONS, tool or verify edits. Nothing in Weir's tree was touched: the module was built there by the lead and copied.

**Why.** So the human can hear and test Weir's real filter on the wavetable voice. Under ADR-307, what is heard must be
the code that ships, so the lab plays Weir's compiled C++ and has no JavaScript copy of the filter.

**Evidence consulted.** horde DECISIONS ADR-307 (filters in Weir, no JS twin) and ADR-311 (`prepare(sr, 4096, 0.0)`: no
smoothing; per-sample cutoff modulation in octaves, unsmoothed); SPEC-ENGINE RC-1 (via `include/weir/filter.h`'s notes:
the slot after the governing envelope, the same coefficients on both channels, the tail rule `tail() < 1e-4`). Weir, read
only: `include/weir/filter.h`, `include/weir/svf.h` (modes, the damping law, Ring's floor, the energy law),
`lab/weir_abi.cpp` (the C ABI, 16 slots), `lab/worklet.js`, `lab/wasm_env.mjs` and `lab/lab.js` (synchronous
instantiation in the worklet from a module the page compiled; three inert WASI imports), `lab/build.sh` (flags),
DECISIONS D-013 and D-015. The B650 to B652 traces.

**How it is built.**
1. **One module instance in the worklet, 16 filters inside it.** The page fetches and compiles `weir/weir_lab.wasm` and
   passes the module in `processorOptions`. The worklet instantiates it synchronously (no Emscripten glue) and creates
   16 SVF 12 instances (Weir's ABI has 16 slots): bank 0, 8 voices at the sample rate, `maxBlock` 128; bank 1, 8 voices
   at 2× for the FM oversampling switch, `maxBlock` 256. Each is `weir_prepare(h, sr, block, 0)`: no smoothing, as
   ADR-311 rules. Memory: one fixed 16 MiB linear memory (the build disallows growth), so the Float64Array views onto
   each slot's left, right and cutoff-mod buffers are made once in the constructor and `process()` allocates nothing.
   The page instantiates the module a second time for the curves (another 16 MiB). The kind is looked up by id
   (`svf12`, Weir D-013's ruled form: Simper, energy law) in Weir's registry, never by index.
2. **The slot.** Per voice: both oscillators summed (stereo) → × the amp envelope (ENV 1) × velocity → SVF 12 (the same
   coefficients on both channels) → the voice sum. The slot is after the amp envelope, matching RC-1's "after each
   source's governing envelope", and the page says so on the Filter card. When the envelope ends, the voice is freed but
   its filter keeps ringing with no input until `weir_tail(h) < 1e-4` (horde's −80 dB free threshold); a new note takes
   a voice whose filter is not ringing first. A fresh note `weir_reset`s its voice's filter.
3. **ABI calls used.** `weir_registry_json`, `weir_create`, `weir_prepare`, `weir_reset`, `weir_left`, `weir_right`,
   `weir_mod_cutoff`, `weir_set`, `weir_process` (`useMod` 1 only when something moves the cutoff), `weir_tail`,
   `weir_response` + `weir_scratch` (curves, page only), `memory`, `_initialize`. Not used: `weir_throw`,
   `weir_process_thrown`, `weir_claim`, `weir_render_fixed`, `weir_destroy`.
4. **Per sample vs per block.** Per sample, through `Mod.cutoffOct` (`weir_mod_cutoff`): Env amount × ENV 2, every
   route to Filter · Cutoff (LFO 1 and LFO 2 are now evaluated per sample in closed form on the audio clock when they
   feed the cutoff; ENV 2 is per sample for every voice), and the knob's own move across each 128-sample block as a
   linear ramp in log2 Hz, so a turned knob does not step at block edges with Weir's ramp off. Per block, by `weir_set`:
   the base cutoff (knob + keytrack about C4 + velocity), resonance, Ring, the mode, and the routes to resonance, Ring
   and Env amount. The lab ABI's `weir_process` passes no resonance buffer (Weir's `Mod` has one), so resonance cannot
   go per sample here.
5. **Rows** (corner values; mode Structural, the rest Morphable): `filter.mode` (Off, LP, BP, HP, Notch, Peak; Off is no
   Weir call at all), `filter.cutoff` (20 Hz–20 kHz, log), `filter.resonance`, `filter.ring` (Weir's `morph`, D-015),
   `filter.keytrack` (0..1), `filter.envAmt` (−8..+8 oct at ENV 2 = 1), `filter.velocity` (−4..+4 oct at velocity 1).
   **No Drive:** SVF 12's registry lists `uses: cutoff, resonance, morph`, and svf.h says drive is unused. Matrix
   destinations: Filter · Cutoff (per sample), Resonance, Ring, Env amount. A new device slider, Key velocity
   (default 0.85), sets the keys' velocity so the Velocity row can be heard; the PF-3 chord stays at 0.85.
6. **Pictures.** The Filter card draws |H| from Weir's `response()` for every sounding voice at its own live cutoff (the
   worklet reports each voice's effective cutoff in its stats) and lists them per note; the Harmonics display draws the
   newest voice's |H| in violet at each harmonic's frequency, with the cutoff's harmonic number.
7. **Presets.** Seven re-voiced (the old version stays behind an A/B button next to the menu, which keeps the puck
   where it is): *Growl bass* (LFO 1 → cutoff too), *Acid squelch* (saw / square through resonant LP, Env amount
   4–5.5 oct, replacing ENV 2 → frame), *80s brass* (Env amount ~3 oct with ENV 2's slow attack, replacing ENV 2 →
   frame), *String machine* (full saws, fixed LP 2–4.5 kHz), *Hollow PWM pad* (LFO 1 → cutoff), *Reese and sub bass*
   (LFO 2 → cutoff, replacing LFO 2 → frame), *Pluck and harp* (Env amount 4.5–6 oct, replacing ENV 2 → frame). Four
   new, group "Filter first (Weir SVF 12)": *Resonant sweep pad*, *Ringing pluck with Ring* (BP at res ≈ 1 keytracked to
   the 2nd harmonic, Ring 0 / 0.5 / 0.6 / 1), *Keytracked bass* (keytrack 1, 1, 0.5, 0 by corner; velocity +1 oct),
   *Notch and peak morph* (LFO 2 carries the morph so the mode flips Notch ↔ Peak). A new patch opens on its filter
   version.

**Measured** (in-app browser, 44.1 kHz, served from this worktree on port 8153).
- *Levels, PF-3 chord retriggered, at each corner, filter versions of all 11 filter patches* (2.6 s each): 44 cases, no
  CLIP latch. Peaks −11.8 to −3.8 dBFS after one level pass (the ringing pluck's band-pass corners first read −27 to −34,
  raised; the sweep pad's band-pass corner raised; the keytracked bass raised 3 dB), then the three re-measured:
  ringing pluck −16.7 to −9.8, keytracked bass −9.1 to −5.6. The patches with slow cycles were measured over a full
  cycle: sweep pad (LFO 1, 7.7–20 s) A −6.5, B −4.6, C −8.5, D −3.6; Reese (LFO 2, 6.7 s) −7.2 to −5.1; notch/peak
  (16.7 s, the mode seen flipping Notch/Peak) −8.5. Resonance ringing is included (the pluck's corners sit at
  res 0.97–1 with Ring up to 1).
- *Tails*: ringing pluck at D (Ring 1), chord held 0.3 s and released: 8 freed voices kept ringing, then freed one by one
  as `tail()` fell (8 → 1 over 2.1 s, the output falling from −36 to −112 dBFS).
- *Modes and routes*: Off, LP, BP, HP, Notch, Peak each played on the chord, no CLIP. LFO 1 → cutoff at depth 0.3 moved
  the C3 voice's reported cutoff 73 Hz–4.41 kHz (±3 octaves as designed); ENV 2 → cutoff moved every voice; the 2×
  oversampling bank played the chord at −9.3 dBFS with live cutoffs.
- *CPU* (opening patch, the heaviest, PF-3 chord, the lab's `Date.now()` estimate averaged over 4 s, two runs each):
  filter Off 0.298 / 0.311; LP static 0.314 / 0.321; LP with Env amount and LFO 2 → cutoff per sample 0.299 / 0.335.
  The difference is inside the estimate's noise. Measured directly (Node 26, the same V8, the same module, the lab's
  call pattern, 20 s of 8 voices): LP with per-sample cutoff modulation 1.81 % of real time (52.6 µs per 128-sample
  block for all 8); LP with no modulation 0.26 %; Weir's bypass 0.09 %. Per-sample modulation costs about 7× the static
  filter, because SVF 12 recomputes its coefficients (a `tan`, `exp2`, `pow`) every sample it moves.

**Checked in the in-app browser** at http://127.0.0.1:8153/wavetable-lab.html. Real clicks and drags: Start audio, Hold
chord, the LP and BP mode buttons, a cutoff drag (150 Hz → 6.49 kHz) and a resonance drag with the chord held, the A/B
button (to "B: old, no filter", which loaded the old harp with ENV 2 → frame and the mode Off, and back), a computer
key. The mode sweep, routes, CPU and level runs were scripted through the page's own functions. The console logged
nothing in any tab, including a fresh load. A bug found and fixed on the way: under "All corners" the filter rows'
inert hints read the resolved mode a tick late, so they now re-sync when the resolved mode or the resonance threshold
changes.

**Not checked: needs the human's ears.** Whether SVF 12 sounds right on these voices; whether a cutoff sweep, ENV 2
and LFO → cutoff are free of zipper; whether a mode flip under the notch/peak patch clicks (it lands on the next sample,
no crossfade); whether the resonant patches are balanced by ear (levels were set by peak, not loudness); whether the
re-voiced patches beat their old versions (A/B); whether a new note on a voice still ringing (only when all 8 are busy
or ringing) cuts audibly.

**Lab limits** (also under About): resonance, Ring and their routes move per block; the base cutoff, keytrack and
velocity per block with the knob's in-block move ramped per sample; a cutoff route is clamped only by Weir (10 Hz to
0.49·fs), not to the knob's range; mode flips have no crossfade; a fresh note resets its voice's filter; switching the
oversampling resets every filter; LP and Notch are not level-compensated by the energy law, so resonant LP patches need
lower oscillator levels.

**Verify.** `nice -n 10 ./verify fast` in this worktree; see the report.
