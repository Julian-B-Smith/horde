# 2026-11-08 — B651: the wavetable lab, version 2

> Origin: horde lead session, 2026-11-08, ROADMAP B651. The human, playing the B650 lab: "A few things I want to test:
> 1. having settings map to corners as well (which will make it more realistic to Horde), and 2. transforming LFO
> mappings, using the LFO point interpolation rule from the lab. There must be a simpler system for spectral log than
> recalculating the tables on every note when there's an envelope on the frame".

**What changed.** One file, `docs/wavetable/lab/wavetable-lab.html`. No spec, reference, ROADMAP, DECISIONS, tool or
verify edits.

**Why.** So the human can hear the wavetable member the way horde will morph it (every setting a corner value), try
drawn LFOs whose shapes and route depths transform with the morph, and play a Spectral-log frame envelope without a
rebuild on every note.

**Evidence consulted.** SPEC-ENGINE 5.1 to 5.3 (classes M, S, D of the rows the lab uses), Appendix D (MO-4 to MO-13,
MO-26, MO-27, D.2's morph settings), C.12 (the segment curve), LF-8, MS-12, MS-29, MS-34; CONTRACT §2.1 (the `wt.*`
classes), §4.1 and §4.2 (destinations; the table key under Blend and Quantum), §7 C6; SPEC-WAVETABLE WT-3, WT-12,
WT-14, WT-22, WT-23; DECISIONS ADR-349. The brief named `docs/design/modulator-lab.html` as the shape reference; that
file has no `MAXPTS` or point editor. The law with `MAXPTS = 32`, parking and the exact bend split is the design
system's `ShapeMorph` (GUI-37) in `docs/design-system/master/components/bundle.js` (shape law `ModLaw.shapeAt`), which
`docs/design/gui3.html` tests; the lab ports that.

**How it is built.**
1. **Settings map to corners.** A parameter registry (`PDEF`) gives every row its class. `ST.cv[key]` holds four corner
   values. A page-side resolver runs at each tick at the heard position: Morphable rows blend at the bilinear weights in
   their normalised form (MO-9, so time rows blend on the log taper), glided by `morph.glide`; Structural rows
   (`wt.law`, `wt.syncIn`, unison, octave, semitone) take their quantum pick under every law (MO-13). The picks are a
   stand-in for MO-6 to MO-8: Gumbel draws from mulberry32 seeded by FNV-1a of `morph/key/<key>` plus the morph seed, a
   score of ln(w)/T + (1 − h)·g_key + h·g_shared, and the latch margin ln(0.55/0.45)/T for Structural keys. `osc2.on`
   fades as a level under Blend (MO-26 (1)). Quantum makes every key pick, with the tables one-hot at the table key's
   pick (CONTRACT §4.2). Device rows (polyphony, output level, the oscillator seeds, the morph settings, the walk, the lab
   switches) stay global. Each control edits the selected corner (A to D, or All). Ticks show the four corner values
   and a pink line the resolved value; Structural switches show corner dots and a ring on the resolved option. Clicking
   near a pad corner selects it and moves the puck there. There are three presets: Opening, where the corners differ
   in frame, spread, sync, window, asym, bend, unison, detune, coupling, width, level, both envelopes, LFO rates and
   shapes, and route depths; Pluck; and Init. The notes export carries `settings.cv` (all four corners), the LFO models
   and the resolved values.
2. **Drawn LFOs and a mod matrix.** There are two LFOs, each one point list (cap 32) shared by the corners, with
   per-corner t, v and u. Blend moves the points by the weights and re-sorts by time, the point id breaking ties.
   Quantum picks per point with the purpose `morph/lfoN/point/<id>`; cohesion 1 is the whole-shape flip. A point added in
   one corner parks in the others by the exact split. The editor shows the edited corner, the other three faintly and
   the resolved shape live with a playhead; points can be dragged, segments dragged to bend, and points added or
   removed by double-click (or removed by right-click). The LFOs are global and free-running; the phase is computed in
   closed form on the audio clock, identically on the page and in the worklet. Six routes run from LFO 1, LFO 2 or
   ENV 2 to 25 per-oscillator destinations and to morph.x and morph.y. Only the LFOs may move the morph; ENV 2 →
   morph is refused and named. Depths are corner values, applied per voice per 128-sample block in the worklet, in each
   destination's normalised form. The old single morph LFO is gone; the walk stays.
3. **Spectral log without per-note rebuilds.** The worker pre-blends all 16 frames under Spectral log (WT-12 at each
   integer frame) into a 16-frame set, the same layout as Linear's (b), so each voice crossfades two adjacent frames
   and the frame, frame spread and ENV 2 → frame are per voice. A lab switch offers three modes: pre-blend all 16 (the
   default), lazy (only the adjacent pairs that sounding voices read; the worker polls the worklet every 15 ms over the
   existing port, the worklet answers from its port handler, never from `process()`, and an unbuilt frame reads its
   nearest built one), and the old shared frame for A/B.

**Checked in the in-app browser** at http://127.0.0.1:8151/wavetable-lab.html (served from this worktree). These used
real clicks, drags and key presses: Start audio; Hold chord; selecting corner B by clicking near the pad's corner (the
resolved values then equal B's exactly: frame 0.7, × pitch, unison 7); dragging Detune at B (only B's value changed,
0.28 → 0.739); a diagonal pad drag (unison flipped 7 → 9, the attack blended on its log taper); dragging an LFO 1 point
in corner B (the resolved point moved from 0.500/0.019 to 0.486/0.076 at B's weight 0.09); selecting D with its button
and double-clicking to add a point there (corner A's curve then differed from before by 1.1e-16 at 201 points, and the
new point parked in A, B and C); the Quantum and Blend buttons; the preset menu (Pluck); route 6's source menu; and
route 3's depth (LFO 1 → morph.x at 0.6 in All corners, after which the heard x swept 0.64 to 1.00). The following were
scripted through the page's own state:
- The routes reach the voices: under the opening chord a voice's frame read 0.390, which is the resolved frame 0.420
  plus 0.108 × the LFO value −0.234.
- At cohesion 1 the LFO's points flip together (00000, 11111, 22222 at three positions). At cohesion 0 they pick
  separately (33310).
- Under Quantum at temperature 1, the table key held corner A at (0.9, 0.1), because its own draw for A is +2.99. This
  is MO-7 as written, and lowering Temperature makes the picks follow the heaviest corner.
- Item 3: rebuilds of osc 1 under the Pluck preset (Spectral log, ENV 2 → frame), from the worker's own timings:

  | mode | morph still: 6 plucks | morph moving (LFO → x, y), 4 notes, 3 s |
  |---|---|---|
  | pre-blend all 16 | 0 sets, 0 frames | 276 sets, 4,416 frames; 7.6 ms mean, 9.1 ms max per set; worker 70 % busy |
  | lazy | 0 sets once warm; at a fresh position the move builds 2 frames (4.5 ms), the first pluck 13 frames in 7 sets, the next plucks 0 | 284 sets, 611 frames (2.2 per set); 1.4 ms mean, 4.3 ms max; 13 % busy |
  | shared frame (old) | 133 sets in 1.9 s (one per page tick while the envelope moves) | 275 sets, 1 frame each; 0.7 ms mean |
  | Linear, for reference | — | 279 sets, 16 frames each by weighted sums; 1.4 ms mean, 2.4 ms max |

  Pre-blending all 16 frames once at a still position took 8.8 to 16.3 ms.
- The heaviest case played eight voices with 9 + 9 members, osc 2 on and the morph moving. DSP load was about 23 % by
  the worklet's `Date.now()` estimate. Every analyser read had zero non-finite samples. The console showed no errors on
  the final load.

**Found while building.** The worker kept the last frames-in-use mask from a previous lazy period, so the first lazy
build after switching back built 14 stale frames. It now clears the mask when lazy mode is entered (re-measured
above: 2 frames at the move).

**Not checked.** How it sounds. Only the human's ears can confirm that there are no clicks when a Structural row flips
(unison, sync and octave step at a block edge here, without PA-5 or PA-6), no zipper as route depths move, that a
Quantum flip glides acceptably, and that the linear crossfade between log-blended frames sounds right (C6). Real key
presses in the pane end too fast to sample the voice; the per-voice frame sweep (0.53 → 0.02 with 0 rebuilds) was
measured on a scripted note.

**Lab limits** (also under About): the resolver runs on the page at about a 30 ms tick; routes are applied per
128-sample block; the LFOs are global only, rates in Hz, drawn kind only; there is no Stepped, Count Two, Exempt,
waypoints, region locks, or MO-26 restriction of blends; and a this-frame capture at a fractional frame bakes the
log-domain rule, so it can differ slightly from what pre-blend plays.

**Verify.** `nice -n 10 ./verify fast` in this worktree; see the report.
