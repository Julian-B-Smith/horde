# 2026-11-07 — B650: the wavetable lab

> Origin: horde lead session, 2026-11-07, ROADMAP B650. The human: "I would also like to throw up a JS lab
> demonstrating the full functionality and design of the wavetable system so we can work through superficial
> details before the deeper build."

**What changed.** One new file, `docs/wavetable/lab/wavetable-lab.html`: a single-file browser lab of the
wavetable member, built to the B649 spec draft. Nothing else in the tree changes: no spec, reference, ROADMAP,
DECISIONS, tool or verify edits.

**Why.** So the human can play the planned oscillator and settle surface details (controls, names, ranges, layout,
feel) before the C++ build. Feedback comes back as data: a notes box per section, exported as JSON with the
current settings and the patch's recipe dictionary.

**Evidence consulted.** `specs/SPEC-WAVETABLE.md` and `docs/wavetable/CONTRACT.md` on branch
`b649-wavetable-spec` at 279d253e (§2.1's ten `wt.*` rows at offsets 0x030 to 0x039, WT-1 to WT-56, §3.1's recipe
format and FNV-1a id, §7's open items); the lead's note on that rework (no `wt.on`, the member's source is hidden
state; K-weighted loudness at C3, peak limit 2.6, +24 dB cap; every level rebuilt each time; Spectral log frames
in the log domain; the factory saw's polarity); `reference/quadrant/quadrant-wavetable.html` (blend math, the
bench's eight generators, the look); SPEC-ENGINE 5.2, 5.3 and E.5 for the existing rows' keys and ranges
(`oscN.unison`, `detune`, `coupling`, `width`, `env1.*`, `env2.*`, `mix.oscN`, `out.level`, `lfo1.rate`,
`xy.tau`, `xy.sigma`); `docs/design-system/master/tokens.json` (colours and type, copied read-only);
`tools/engine_words_check.py` (GUI-1: no engine selection).

**How it is built.**
- Tables are 16 frames × 1,024 harmonics, stored as spectra, peak-normalised per frame at 8,192 points; WT-47's
  gates run on every table (near-silent frame, band energy under 20 kHz at C3 within 12 dB, frames differ) and a
  failing table is refused and named. The bench's eight factory tables come from formulas: hard sync analytically
  from its jumps, FM and fold by FFT at 16,384 and 32,768 points; Sine → saw negated to rise from −1.
- Two workers, one per oscillator, rebuild the published set only on a change of corner, weights, law or (under
  Spectral log) the shared frame, and build every level each time. Linear sums the corners' level sets for all 16
  frames (WT-23 (b)), so the per-voice frame and frame spread are read-time. Spectral log blends one frame in the
  log domain and inverse-FFTs it. Loudness is per WT-16 (exact through the K-weighted Gram matrix under Linear).
- One AudioWorklet (Blob URL) plays 8 voices × 2 oscillators × up to 9 members. Sets arrive by transfer over
  a MessageChannel from each worker and crossfade in over R = 4·round(fs/1512) samples (116 at 44.1 kHz); retired
  buffers go back to their worker from the port handler, so `process()` posts nothing and allocates nothing
  (stats are polled: the page sends a buffer, the handler fills it and transfers it back). Levels are one per
  octave, N = min(8192, max(2048, 16·H)), chosen per member for its read rate (sync ratio and warp slope
  included) under c = min(20 kHz, 0.45·fs), with WT-5's raised-cosine mix in each range's upper third. The read
  is 4-point cubic by default, 6-point Lagrange on a switch.
- The member chain is sync (with a polyBLEP on the restart step) → asym/bend warp → PM from osc 2 (its member
  i mod U₂, times osc 2's amp envelope and on-fade) → read → window. The swarm is a sketch: even detune, balanced
  seats, seeded start phases, plain Kuramoto mean-field coupling.
- Determinism: no `Math.random`; mulberry32 streams with visible seeds (start phases, S&H LFO, walk, random
  table, keyed by purpose under `wt/random/`). Morph motion runs on the audio clock.

**Checked in the in-app browser** (served with `python3 -m http.server 8150 --bind 127.0.0.1 --directory
docs/wavetable/lab`; `tools/serve_labs.py` serves only `docs/design` and `reference`). Real clicks on Start
audio, the hold-chord button, the pad (a drag) and the law button; a real key press started audio with no
button. Scripted through the page's own state with the PF-3 chord held, each case measured from the analyser
(peak, RMS, non-finite count) and the worklet's stats:
- every case read zero non-finite samples and was audible: Linear, Spectral log, frame envelope, sync × pitch
  3.5, window 1, sync Hz 1500, asym 0.8 with bend −0.5, PM 4 from a silent osc 2, osc 2 at −6 dB, 9 + 9 members
  at coupling 0.8, 6-point read, half-octave levels, the LFO on x and y, the 2-D walk;
- the level choice moved as specified: A4 reads H 32, sync ×3.5 drops it to H 8, the warp to H 16, half-octave
  spacing showed H 45 at 97 % toward the darker level;
- a static patch: the rebuild count did not move over 2.5 s; a Spectral-log pluck rebuilt 50 times in 1.5 s as
  its frame envelope decayed; Linear rebuilds take 2 to 26 ms after fusing the blend into one pass (they took
  10 to 100 ms before);
- under the LFO the normaliser's gain moved between +4.6 and −0.4 dB while output RMS stayed within about 1.5 dB;
- capture: a 16-frame capture, a this-frame capture of it (depth 2), the same recipe again not duplicated, both
  rebuilt bit-identically from their recipes, and rebuilt from recipes alone after a reload; a two-step delete
  of a table in use left the corner playing and marked "not in library"; a random table at tilt +4 dB/oct was
  refused by the band gate (−17.7 dB);
- DSP load (the worklet's own `Date.now()` estimate): 5 % for the opening chord, about 30 % at 9 + 9 members,
  about 44 % with the 6-point read;
- the console showed no errors on the final load. Layout was checked in headless Chrome screenshots, dark and
  light, at 1,360 px; the pane is about 700 px wide and stacks as designed.

**Not checked.** How it sounds. Analyser probes cannot see audio-thread underruns, so only the human's ears can
say there is no crackle, no click on motion and no zipper on slider moves. Aliasing, top-of-band, click and CPU
gates (WT-F1, F2, F4, P1) are the C++ build's, not measured here.

**Lab limits** (stated on the page under About): sets are published at most about 60 times a second, not at
every rebuild boundary; the slope corner of the sync restart (ED-6) is not corrected; DC is removed by one 8 Hz
blocker; parameters are smoothed per 128-sample block; one LFO with a Y phase offset stands in for the drawn LFO;
only Blend of the morph's laws; the library lives in browser storage as recipes.

**Found while building.** The random generator's first tooltip said a flat tilt fails WT-47's band gate. It does
not: a flat spectrum to 1,024 harmonics keeps about −8.3 dB of its energy under 20 kHz at C3, inside the 12 dB
gate. The gate starts refusing at about +1.5 dB/oct. Corrected before commit.

**Verify.** `nice -n 10 ./verify fast` in this worktree; see the report.
