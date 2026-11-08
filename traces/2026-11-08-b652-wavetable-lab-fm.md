# 2026-11-08 — B652: FM/PM in the wavetable lab, SCALPEL's jitter, and a preset bank

> Origin: horde lead session, 2026-11-08, B652. The human, playing the B651 lab: "how do you think we could recreate the
> 'operator jitter' or 'trance jitter' effects from scalpel? … a sawtooth is subtly pitch-modulated by a high frequency
> noise LFO. … Could we add FM/PM to this bench to see how it sounds?" Then, in the same batch: "Could we also get some
> presets in the lab? I want to try a diversity of morphs and sounds and envelopes and transforms and LFOs across the
> whole thing, and possibly some attempts at recreating traditional synth sounds."

**What changed.** One file, `docs/wavetable/lab/wavetable-lab.html`, plus this trace. No spec, reference, ROADMAP,
DECISIONS, tool or verify edits.

**Why.** So the human can hear FM/PM on the wavetable member (including SCALPEL's jitter patches) and walk a bank of
patches that exercise every part of the lab.

**Evidence consulted.** `reference/scalpel/prototype/razor-core.js`: `RazorCore.hash`, `.wave`, `.mod` (lines 10–40),
`fmStep` (pitch law, line 163), `voice` (PM and the blade depth, lines 168–196), `stepM` (the modulator's phase, line
659), the per-member `mEff` (lines 833–838) and the start state in `startVoice` (modX = 1000·Math.random(), seed i·97);
`reference/scalpel/prototype/scalpel-bench.html` (the presets at 1616, 1630, 1636, 1653; the FM depth readout
±1200·log2(1 + 0.1·I) at 1335); `reference/scalpel/data/presets.json`; `specs/SPEC-WAVETABLE.md` WT-30 to WT-39
(WT-36's level choice from the interval's maximum read rate); `docs/wavetable/CONTRACT.md` §2 (osc1.wt.pm 0x1039,
osc2.wt.pm reserved, §7 E2); the B650 and B651 traces.

**How it is built.**
1. **FM/PM section, per oscillator, every value a corner value.** Source: Internal | Osc 2 (osc 1 only; this is WT-30's
   PM and it absorbed the old `osc1.wt.pm` slider) | Self. Type: Phase (the read phase moves by I/2π × m, I in radians)
   | Pitch (SCALPEL's law: the read rate × (1 + 0.1·I)^m, integrated in a phase accumulator, so ±1200·log2(1 + 0.1·I)
   cents at full swing). Internal modulator: seven shapes (Sine, Tri, Saw, Rev saw, Square, Smooth noise, S&H noise),
   ported exactly from `RazorCore.mod` (the noise is SCALPEL's sine hash at index floor(modX) + seed, member seed i·97,
   the phase wraps at 65,536, cached per index so it costs two `Math.sin` per noise point, not per sample); rate as a
   Ratio of each member's frequency (keytracked) or fixed Hz; FM mix (SCALPEL's blade depth: a second, unmodulated read
   mixed in); per member Decorrelated (own seed and start) or Shared (one modulator per voice at the oscillator's centre
   pitch). Self uses the mean of the member's last two outputs, clamped to ±1, and clamps the PM index at 1.6 rad (the
   UI says so). The only change from SCALPEL: its `Math.random()` start phases are mulberry32 draws from
   `osc seed ^ FNV-1a("wt/fm/modulator")`, and a periodic shape at a ratio starts locked to its member's carrier.
   Index, ratio, Hz and mix are matrix destinations for both oscillators.
2. **Aliasing.** WT-36: each member's level comes from its maximum read rate with FM included (Pitch: × (1 + 0.1·I)^max|m|;
   Phase: + I/2π × the modulator's peak slope × its rate), and the mip ladder outlines in pink where a voice would read
   without FM ("FM moved it from H n"). A new lab switch, FM oversampling 2×, runs every member at twice the rate and
   decimates the voice sum through a 63-tap halfband (Kaiser β 8).
3. **Scope.** Under each oscillator's FM section: the worklet's own ring of the modulator and the resulting pitch
   deviation in cents for the newest held voice's first member, with "now", the extremes since load, the index's bound,
   the modulator's rate and the read-rate factor used for the level. Filled by the poll handler, so `process()` still
   posts nothing and allocates nothing (every new buffer is made in the constructor).
4. **Preset bank: 24 patches in five groups**, each setting every corner value, its corner tables (factory by name, or
   three captures built from their recipes), the routes, both LFO shapes and the puck, with a one-line description under
   the menu. The opening patch is now "Opening · horde morph".

**How each jitter patch maps from SCALPEL** (corner A is the SCALPEL patch exactly; the puck starts on A):

| SCALPEL | lab corner A |
|---|---|
| Trance jitter: base saw, mode 2 (free FM), w 1, depth 1, fmType 1 (pitch), mshape 5, I 0.12, m 3, N 1 | Sine → saw at frame 16 (the 1/n saw); FM Internal, Pitch, Smooth noise, Ratio ×3, I 0.12 (±20.7 ct), mix 100 %, unison 1; ENV 1 4 ms / 400 ms / 85 % / 280 ms (SCALPEL's defaults) |
| Operator jitter: as above, mUnit 1, mHz 700, depth 0.6 | as above, Fixed Hz 700, mix 60 % |
| Jitter swarm: I 0.2, m 3, depth 0.7, N 5, detune 9 ct, K 0.1 | Ratio ×3, I 0.2 (±34.3 ct), mix 70 %, unison 5, detune 0.09 (±9 ct), coupling 0.1 (the lab's plain Kuramoto, not SCALPEL's coupling law) |

The other corners vary: Trance B I 0.3 (±51 ct), C S&H, D 9 members; Operator B ±51 ct, C S&H at 2.4 kHz ±16 ct
(SCALPEL's Crunch), D 7 members; Swarm B one shared modulator, C S&H, D 9 members.

**Measured.**
- *Pitch deviation against SCALPEL* (2 s at A3 = 220 Hz, 48 kHz, first 0.2 s skipped; the same per-cycle estimator on
  both: the period between falling edges, parabola-refined, in cents from nominal). SCALPEL rendered in Node from
  `razor-core.js` with `verify/rng.js` seeded; the lab rendered offline in the page (an OfflineAudioContext running the
  lab's own worklet and workers). Operator jitter and the swarm are measured at mix 100 % and one member, because the
  estimator needs one saw edge per cycle; I = 0 is the must-read-zero control.

  | patch | SCALPEL sd / min / max (ct) | lab sd / min / max (ct) |
  |---|---|---|
  | Trance jitter | 6.58 / −16.5 / +17.4 | 6.25 / −15.8 / +16.5 |
  | Trance jitter, I 0 (control) | 0.80 / −1.7 / +0.7 | 0.22 / −0.4 / +0.2 |
  | Operator jitter, mix 1 | 6.24 / −20.1 / +17.4 | 6.10 / −16.1 / +16.3 |
  | Operator jitter, mix 1, I 0 (control) | 0.79 / −1.7 / +0.7 | 0.22 / −0.4 / +0.2 |
  | Jitter swarm, 1 member, mix 1 | 10.62 / −27.9 / +31.3 | 10.34 / −26.5 / +27.5 |

  A per-cycle estimator averages the 3×-note noise over a cycle, so these sit well under the ±21 and ±34 ct bounds; the
  instantaneous deviation the worklet reports reached −20.6 / +20.6 ct (Trance, Operator) and −34.3 / +34.3 ct (Swarm)
  over 2.5 s of the PF-3 chord, against bounds of 20.65 and 34.28.
- *Offline 2 s renders, one note A3, corner A*: Trance jitter peak −21.2 dBFS, RMS −27.7; Operator jitter −21.5 / −28.4;
  Jitter swarm −19.4 / −30.4; 0 non-finite samples in each.
- *2× oversampling*: the 2× render lags the 1× one by 15.5 samples (the halfband's delay; correlation 0.996), so it is
  engaged. At C7 with PM index 8 at ratio 3.5 (the bell's corner B), the energy away from the expected sidebands fell from
  −1.2 dB of the total at 1× to −10.4 dB at 2× (the index-0 control reads −36.1 dB, the table's own harmonics).
- *Every preset, live at 44.1 kHz with the PF-3 chord held, at each corner (puck on A, B, C, D; patches whose LFOs move
  the morph were measured moving)*: 96 cases, no CLIP latch, 0 non-finite samples, every one audible. Peaks −11.9 to −1.0
  dBFS; after this run the organ (−1.8 at B, −1.0 at D) and the supersaw's locked corner D (−3.6) were lowered and
  re-measured: organ B −6.5, D −7.4; supersaw D −3.0, then osc 1 at D a further 5 dB down: −6.8 (3 s, chord held). The opening patch, moving by itself, peaked at −4.2 over 3 s. DSP load
  (the worklet's `Date.now()` estimate) 0.02 to 0.36; the opening patch is the heaviest.

**Checked in the in-app browser** at http://127.0.0.1:8152/wavetable-lab.html (served from this worktree). Real clicks
and drags: Start audio; the Patch menu (Trance jitter); Hold chord; the S&H noise, Fixed Hz and Phase (PM) buttons in
corner A (the readout then showed the modulator at 1.32 kHz → 700 Hz, and the index as 0.12 rad); a diagonal pad drag
under Jitter swarm (heard 0.85, 0.72; the cents readout ±34.2). One stray click landed on the index slider (8.08) while
the page shifted under a stale screenshot; it was set back by script. The 24-patch level sweep and the offline renders
were scripted through the page's own functions. A fresh tab (Start audio, Hold chord, four patch loads) logged nothing to the console; an earlier tab's errors came from my own offline-render harness leaving the page's `ctx` null, not from the lab.

**Not checked.** How it sounds. Only the human's ears can say whether the three jitter corners sound like SCALPEL's
originals (the numbers say the modulation matches; SCALPEL also runs 2× oversampled with its own DC and edge handling,
the lab reads a band-limited table), whether there is aliasing at high notes with deep PM (the 2× switch is there to
compare), whether the patches are good, and whether Structural flips click.

**Lab limits** (also under About): the level choice ignores the jumps of square and S&H modulators under Phase; under osc
2 or Self the modulator's slope is estimated; the sync polyBLEP ignores FM; routes move FM values per 128-sample block;
ratio FM with a periodic shape is key-synced where SCALPEL's is not. There is no filter and no noise source, so Acid,
Brass and the plucks scan Sine → saw with ENV 2 instead of a filter, and the flute's "breath" is smooth-noise PM at 3 kHz;
their descriptions say so.

**The bank** (group: name · what the pad does).
- Start: *Opening · horde morph* — LFOs move the morph; A wide saw swarm, B hard-sync lead, C Spectral-log vowel pad with
  jitter, D FM table under silent-osc-2 PM; unison, sync and the law flip as it crosses. *Init* — four equal corners.
- SCALPEL's jitter: *Trance jitter*, *Operator jitter*, *Jitter swarm* (above).
- FM / PM: *Digital electric piano* (ratio-1 PM, ENV 2 → index; tine from osc 2 at C, D); *FM bell* (ratios √2, 3.5,
  2.76, 5.19); *Growl bass* (osc 2 PM, LFO 1 → index; C pitch FM from osc 2).
- Classic sounds: *Acid squelch*, *80s brass*, *String machine*, *Drawbar organ* (one registration per corner),
  *Hollow PWM pad*, *Sync lead*, *Hoover-ish lead*, *Reese and sub bass*, *Pluck and harp*, *Choir and vowel pad*,
  *Breathy flute lead*, *Supersaw*.
- Motion and transforms: *Fixed formants* (sync Hz + window), *Comb scanner* (frame spread, law flip), *Fold machine*
  (asym/bend under both LFOs; pad, pluck, stab and swell envelopes by corner), *Capture morph* (three captures built
  from recipes, one a capture of captures).

## Approximating SCALPEL's presets with the wavetable system

A paper estimate for the lead, not a build. `presets.json` holds 83 presets; grouped by mechanism:

| family (count) | examples | can the wavetable system reproduce it? |
|---|---|---|
| FM free / pitch jitter (10) | Trance, Operator, Jitter swarm, Crunch ×4, Crunch pluck lead | **FM/PM, yes** for the whole-cycle ones (w 1): measured above. The two-blade and partial-window ones (Crunch and fold, Two FM voices, FM bell at w 0.25) only in part: the jitter yes, the blade window no. |
| static sync blades (12) | Quarter sync, Harmonic stack, Hollow twin, Reflected sync, Locked horde | **A baked table** per patch: a static blade is one fixed cycle, so a 16-frame generator that renders SCALPEL's cycle across one blade parameter (w, c or k) would reproduce it exactly at rest; none exists in the factory set. The lab's own sync + window acts on the whole cycle, not a slice, so it is only a cousin. Member spreads (kspread, kRule) map onto frame spread, approximately. |
| FM inside a blade (7) | Chord of formants, Fixed formant, Vowel choir, Two-formant vowel | **Sync/window** for the fixed-Hz formants (WT-32's Hz sync with WT-33's window on a sine table is the same idea), else a baked table; chord rules (kRule) need per-member ratios, which the swarm does not have. |
| fold / ring (7) | Folded slice, Ring saw, Golden bells, Metal pair | **A baked table** if static (the factory fold table is a whole-cycle cousin); the inharmonic ring rules (golden ratio) need per-member ratios: not at all. |
| crush (5) | Fixed crush, Slewed crush, Crushed bells | Pitch-locked crush: **a baked table**. Fixed-kHz crush (lock 2) aliases by design at a fixed rate against the note: **not at all** with band-limited tables. |
| blade envelopes (7) | Blade pluck, Zap bass, Split envelopes, Glass FM | **ENV 2 → frame over a baked table** of the swept parameter (k or w), or ENV 2 → the lab's sync ratio for a k sweep: approximate, the lab already does this shape of thing. |
| moving / randomised blades (10) | Undertone drift, Wandering blades, Humanized, Chasing blades, Two clocks | Rotation is a frame LFO over a baked table of blade positions, approximately (16 frames is coarse for a full turn). Per-note random laws and drift: **not at all** (the morph has no per-member random draws of table parameters). |
| cross-mod / feedback (14) | Cross-mod ring, Feedback screech, Screamer, Glass horde pad | **Not at all** for member-to-member cross-mod (xm); feedback in part through FM source Self, which is phase feedback on the read, not SCALPEL's feedback into the blade. |
| serial interplay / collision (7) | Fold over sync, Collision chirps, Occlusion sweep | Static serial pairs: **a baked table**. Collision chirps (pitch moves while the blades overlap): **not at all**. |
| sine-to-saw blades (2) | Sine-to-saw fan, Swarm-linked spread | **Frame spread over a baked table** of the morph amount: close. |
| noise blades (2) | Breath and bite, Breathing pad | **Not at all**: a table has no per-cycle randomness and the lab has no noise source. |

Roughly a third (the jitter family, the static and envelope-swept blades) is reachable with FM/PM plus a "SCALPEL blade
cycle" table generator; another third only approximately; the cross-mod, collision, noise and random families are not.

**Verify.** `nice -n 10 ./verify fast` in this worktree; see the report.
