# 2026-11-08 — B661: sync always on (ratio 1 is no sync) and sync softness, in the wavetable lab

> Origin: horde lead session, 2026-11-08, B661. The human: "what would the computational consequences be of keeping sync
> always on at 1x so it morphs more gracefully?" and "could we add a slider for sync's hardness/softness as in Serum (tapered
> edges on full soft, hard cut on full hard)".

**What changed.** One file, `docs/wavetable/lab/wavetable-lab.html`, plus this trace. No spec, reference, ROADMAP,
DECISIONS, tool or verify edits. The spec is unchanged: this is the lab version the human hears before the spec moves.

**Why.** So the human can hear sync that never switches (a morph from a ratio-1 corner to a ×3 corner sweeps instead of
flipping a Structural row part-way), and a Serum-style hard-to-soft control on the restart, and see what each costs.

**Evidence consulted.** `specs/SPEC-WAVETABLE.md` WT-24 (a table read bypasses ED-1's filter E except while sync is on),
WT-32 (frac(r·θ), r = `wt.sync` or max(1, `wt.syncHz`/f)), WT-33 (the window g = (1 − a) + a·½(1 − cos 2πθ), live only
while sync is on), WT-36 (the level from the peak read rate, the sync ratio included), WT-37 (aliasing per
transformation), WT-38 (`wt.syncIn` Structural, by PA-5's crossfade), WT-39 (cost when off); the B650, B651, B652 and
B659 traces; the lab itself (the worklet's `renderOsc`, the polyBLEP at the restart, the resolver, the preset bank).

**How it is built.**
1. **Sync always on.** `wt.syncIn` is × pitch | Hz (no off); `wt.sync` defaults to 1. At ratio 1 the read phase is
   1·θ − floor(1·θ) = θ exactly, so the read is the unsynced read; Hz reads max(1, Hz/f), so a note above the sync
   frequency is unsynced too. The restart's polyBLEP (every member is one sample late, as before, on every path) now runs
   only when the restart has a value step: never at an integer ratio (pe = 0 restarts on the table's own wrap), and
   scaled by what the edge law leaves of the step. There is no always-on filter. The worklet's smoother now lands on a
   target within 1e-12 (relative) of it, so a ratio gliding back to 1 becomes exactly 1 (a one-pole step there is under
   half an ulp, so it used to stop one ulp away).
2. **Sync softness** (`wt.syncSoft`, a lab row with no CONTRACT offset, class M, a corner value, a matrix destination). The
   read is multiplied by a Tukey window over the member's cycle θ: 1 − depth·½(1 + cos(πθ/τ)) within τ = s/2 of either end,
   1 between. s = 0 is the hard cut; s = 1 is a full Hann window (the old `wt.window` at 100 %). The taper is value- and
   slope-continuous at its joins (at θ = τ the cosine term is 1 with zero slope).
3. **The depth fade-in.** depth = clamp((r − 1)/K, 0, 1), **K = 0.25** by default (a lab switch: 0.25, 0.5, off = depth 1).
   At ratio 1 there is no step to hide and a taper would notch every cycle, so softness does nothing there and engages
   smoothly as the ratio leaves 1, fully by ×1.25. The polyBLEP corrects the step the taper leaves, (1 − depth)·step.
4. **Edge law switch.** "Tukey softness (new)" (default) or "window crossfade (WT-33)" (the `wt.window` row, at the same
   depth). The row of the law not selected is shown inert with the reason.
5. **Sync model switch** for A/B: "always on (new)" or "switchable (old)". The old model reads a corner at × pitch with
   ratio 1 as the old "off" and takes the sync row Structurally by the corner the row's own pick chose (the resolver now
   records each Structural key's picked corner), with no fade-in; the worklet's code 0 is the old sync-off path, kept.
6. **Picture.** The "Blended cycle" display draws, when sync acts, one member cycle of the synced read (the table's cycle
   faint behind it) and the edge law as a dashed envelope ±g(θ), with the ratio, the note, the law's amount and the depth
   on its readout. The picture's gain function mirrors the worklet's formula with `Math.cos`.
7. **Self-checks on the page** (Lab switches · Run sync self-checks): offline renders of the page's own worklet source in
   an `OfflineAudioContext` with fresh table workers, compared sample for sample, and the cost timings below. The results
   go into the notes export (`syncChecks`).
8. **Presets.** Every old patch's sync-off corners now hold ratio 1 (Opening: `sync` [1, 3.2, 1, 1]; the patches that never
   set sync take the new default 1); the three patches that used the window also set the same softness. New group "Sync,
   always on (B661)": *Sync sweep lead* (A ×1 hard, B ×4 hard, C ×2 soft 50 %, D ×6.5 soft 100 %; both LFOs carry the
   morph; ENV 2 swoops the ratio), *Soft-sync pad* (9 members, ×1.2 to ×3 at 60–100 %), *Soft formant voice* (Hz sync on a
   sine at 380 Hz–2.2 kHz, 40–90 %, osc 2 a second formant). 31 patches in seven groups.
9. **Export.** `version: {lab: "B661", sync: …}` marks the change (an older export's `wt.syncIn` is off | × pitch | Hz and
   has no `wt.syncSoft`); `syncSwitches` and `syncChecks` are added; a notes section "Sync always on & softness".

**Measured** (in-app browser, Chromium, 44.1 kHz, served from this worktree on port 8154).
- *Neutrality* (the self-check, run by a real click; 0.6 s of the PF-3 chord; osc 1 at 9 members with pitch FM, smooth noise
  ×3, I 0.12; osc 2 at 3 members an octave down; Sine → saw at frame 16; every case against the old sync-off path, 52,920
  samples, reference RMS audible):

  | case | expected | result |
  |---|---|---|
  | the old sync-off path again (determinism control) | identical | PASS, bit-identical |
  | always on, × pitch at ratio 1 | identical | PASS, bit-identical |
  | ratio 1, softness 100 % (fade-in 0.25) | identical | PASS, bit-identical |
  | ratio 1, window 100 % under the window law | identical | PASS, bit-identical |
  | Hz at 50 Hz, softness 100 % (r = max(1, 50/f) = 1) | identical | PASS, bit-identical |
  | control: ratio 1.0001 | differ | PASS, 52,918 differ, max \|Δ\| 3.3e-3 |
  | control: ratio 1, softness 100 %, fade-in off | differ | PASS, 52,918 differ, max \|Δ\| 0.68 |

  Bit-identical, not merely within rounding. Honest about the mechanism: at ratio 1 the phase math runs (1·θ − floor is θ
  exactly in IEEE arithmetic) and the restart's correction is skipped because its step is exactly 0; the taper is skipped
  because its depth is exactly 0, which is a branch, so the softness and window cases prove the depth law, not the taper's
  arithmetic. The two controls show the checker sees a difference when one exists.
- *Cost, offline* (the same self-check: 4 s of the PF-3 chord, osc 1 at 9 members, FM and osc 2 off; one warm-up, three
  interleaved runs, the median, as % of real time on this machine):

  | case | ms / 4 s | % real time | × off |
  |---|---|---|---|
  | sync off (the old path) | 322 | 8.05 | 1.00 |
  | always on, ratio 1 | 329 | 8.21 | 1.02 |
  | ratio 1.0001, softness 50 % (cut, correction, taper all running) | 358 | 8.94 | 1.11 |
  | ratio 3, hard (integer: no step) | 322 | 8.04 | 1.00 |
  | ratio 3.5, hard (a corrected step every restart) | 323 | 8.07 | 1.00 |
  | ratio 3.5, softness 50 % | 348 | 8.70 | 1.08 |

  Always-on at 1× costs about 2 % of the member loop (a multiply and a floor per sample); the runs were tight (off 321–323,
  1× 328–333). Sync itself at ×3 costs nothing measurable here, partly because a higher read rate reads a smaller mip level.
  The taper costs about 8–11 % of the loop while it is in its tapered region (a cosine per tapered sample).
- *Live DSP-load estimate* (the worklet's `Date.now()` readout, the same patch on the live context, chord held, 4 s averages,
  two rounds): off 9.1 / 9.6 %; always on ×1 9.5 / 9.4 %; ×3 9.3 / 9.4 %; ×3.5 at softness 50 % 10.4 / 9.6 %. Inside the
  estimate's noise, as expected from the offline numbers.
- *The morph, 1× corner → 3× corner* (Init with `sync` [1, 3, 1, 3], x swept 0 → 1 at y 0 in 0.01 steps, glide off): always
  on: no flip, the ratio reads 1, 1.32, 1.73, 2.28, 3 at x 0, ¼, ½, ¾, 1, the largest step 0.033 per 0.01 of x; switchable
  (old): one flip at x 0.73, from unsynced straight to ×2.23 (a step of 1.23).
- *Aliasing of short tapers* (offline, C6 = 1,046.5 Hz, 42.1 samples a cycle, ratio 3.5, one member; the inharmonic share of
  the energy in a Hann-windowed 16,384-point FFT, ±4 bins of each harmonic counted as harmonic): no sync (control) −48.5 dB,
  the estimator's floor; ratio 3 hard −46.8; ratio 3.5 hard (polyBLEP) −32.3; softness 0.02 −29.8; 0.05 −28.6; 0.1 −32.7;
  0.3 −47.0; 1 −47.8. At 2× (the FM oversampling switch): hard −44.2; 0.02 −41.6; 0.05 −45.2; 0.1 −45.9; 0.3 −47.9;
  1 −47.8. So a taper shorter than about a sample a side (s ≲ 0.05 at C6) aliases 2–4 dB more than the corrected hard cut,
  because it takes the step the polyBLEP would have corrected and is itself a step at that length; s ≥ 0.3 is clean at 1×;
  2× brings every case to −42 dB or below.
- *Levels* (live, PF-3 chord retriggered, ~3 s per corner): Sync sweep lead A −10.0, B −9.4, C −11.3, D −11.8 dBFS;
  Soft-sync pad −12.6, −11.9, −11.8, −9.6; Soft formant voice −8.9, −6.8, −9.9, −8.0; the translated Opening −8.0, −10.2,
  −9.0, −3.8; Sync lead −8.2, −5.4, −10.2, −6.4; Fixed formants −8.8, −10.0, −8.2, −8.0. No CLIP latch, no non-finite
  sample in any. Spot checks of untouched patches (Growl bass, Acid, FM bell, Capture morph, Ringing pluck) played with no
  CLIP; the Ringing pluck read −9.1 / −11.0 in the new model and −12.9 / −12.0 in the old (pluck transients; B659 read
  −16.7 to −9.8).

**Checked in the in-app browser** at http://127.0.0.1:8154/wavetable-lab.html. Real clicks and drags: Start audio; the
Patch menu (Sync sweep lead, by form input); Hold chord; corner A's Sync ratio dragged 1 → 2.64 (the softness row's "ratio ×1
is no sync" hint then cleared) and its Sync softness dragged 0 → 0.667 (only corner A changed: [0.667, 0, 0.5, 1]); the
Sync edge law and Sync model buttons both ways (the fade-in row went inert under the old model, with its reason); a pad drag
(puck to 0.79, 0.20); Run sync self-checks. Scripted through the page's own state: route 4 LFO 1 → Osc 1 · Sync softness at
depth 0.5 (the page's mirror swept 0.06 → 0.77 over 4 s), the export (version, switches, the new row in `settings.cv` and
`resolved`), the morph sweep, the levels and the measurements above. The cycle picture showed the ×3.65 synced read over the
faint saw with the dashed envelope. The console showed no errors.

**Spec consequences for the lead** (nothing here changes the spec):
- WT-24 says filter E engages "while sync is on". With sync always on, that would put E (and its 2.4-sample delay) on every
  table member all the time. The lab's rule is narrower: correct the restart only when it has a step (a non-integer ratio,
  times what the edge law leaves); WT-24 would need the same wording, and E's engage/release would then follow the ratio
  crossing an integer, which needs its own transition rule.
- WT-38: `wt.syncIn` stays Structural for × pitch ↔ Hz; a × pitch ↔ Hz flip still jumps.
- WT-39: always-on at 1× costs ~2 % of the member loop in the lab; an engine could skip the phase math per control interval
  when r = 1 exactly, making it free, at the price of a branch.
- WT-33 vs softness: the human picks the law; the fade-in constant (0.25) is a lab choice to rule on.
- Softness aliasing: WT-37's "never needed for the restart" holds for the corrected hard cut and wide tapers, not for a taper
  under ~1 sample a side; the engine would either floor τ at a couple of samples or correct the residual step.

**Not checked: needs the human's ears.** Whether the 1 → 6.5 sweep is graceful (no zipper as the ratio passes integers, no
click as the depth fades in); how softness feels from hard to soft, and whether 0.25 or 0.5 is the better fade-in; whether
the Tukey law or the window crossfade sounds better between 0 and 100 %; whether short tapers at high notes audibly alias at
1× (the numbers say slightly more than a hard cut at s ≲ 0.05); whether the old model's flip is as audible as its numbers.

**Lab limits** (also under About): the slope corner (ED-6) is still not corrected; the taper is not band-limited; the
picture leaves out the warp and FM; routes move softness per 128-sample block; the polyBLEP is computed without FM.

**Verify.** `nice -n 10 ./verify fast` in this worktree; see the report.
