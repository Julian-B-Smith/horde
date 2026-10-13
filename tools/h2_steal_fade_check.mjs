/*
 * h2_steal_fade_check.mjs — ADR-216 D7 (`stealFade`, ROADMAP B459) does what it was
 * ruled to do, on the C++ engine: with the flag on, a chord that takes sounding
 * voices makes no jump in the output, the old sound ends through a fade that keeps
 * its shape from its first sample to its last, its notes start when the fade ends and
 * from silence, a held note is never taken while a released one sounds, and the flag
 * adds no dependence on the host's block size; with the flag off (the present law)
 * the same chords read the jump.
 *
 * WIRED: ./verify full
 *
 * THE FAULT (B459). With the pool full a poly note-on takes a voice that is still
 * sounding (the quietest releasing tail, or the oldest held voice) and restarts it
 * as a fresh voice: the envelope keeps its level while the phases, filters and DC
 * state reset, so the output jumps in one sample. D7 fades the taken voice over
 * 0.005 s and starts the note from silence at the sample the fade ends
 * (h2/engine/engine.h, take and startWaiting).
 *
 * WHAT IS RENDERED. Everything goes through the engine harness (h2/harness/), so
 * this is the C++ engine as it ships, never the JS one. The lead's measurement
 * (ROADMAP B459): `ledger/breathing-pad`, 48 kHz, six triads, each held 1.6 s and
 * struck 0.1 s after the last release, at a pool of 6 and of 8 voices. The pad's
 * release is 3 s, so the pool of 6 is full after two triads and every chord from
 * the third on takes three voices. At 8 the third chord takes one, the rest three.
 *
 * With the flag on a stolen note is late by its wait in every respect, so each chord
 * that waited is also RELEASED 0.005 s after its keys are lifted (its notes sound for
 * the 1.6 s they were held). The render with a chord and the render without it share
 * that history, so no row reads it as a difference.
 *
 * THE JUMP at a chord is the largest |difference|, over the first four samples
 * after its note-ons, on either output channel, between a render WITH the chord
 * and one WITHOUT it (the same history, the same flag), as a share of the rms of
 * both channels over the 50 ms before. Before the note-ons the two are the same
 * samples, so the difference is what the note-ons did.
 *
 * THE SETTINGS ARE PINNED HERE, NOT LEFT TO THE BANK (as tools/h2_frame_hold_check.mjs
 * does, and for its reason: ADR-217 edited the bank's pad to detune 0 and K -0.3
 * after the measurement). Seven are the values the lead measured under: `detune`
 * 14, `K` 0.45, `law` 0 (the spread law off), `A` 1500, `S` 0.9, `R` 3000, `N` 6.
 * The pool (`poly`) is set by each job.
 *
 * AN EIGHTH PIN, AND WHY: `b2on` 0 (FOUND BUILDING THIS CHECK, 2026-10-13). The
 * pad's second blade is a noise blade (`mode2` 3), and a noise blade draws from
 * the engine's shared random stream on every noise step while it renders. A
 * note-on draws from the same stream. So a render WITH a chord and one WITHOUT it
 * hear different noise from the note-on onwards, whether or not a voice is taken,
 * and the jump reads that difference too: the confound the row names for spread
 * law 4, by another route. Measured with the seven pins and blade 2 on: a chord
 * that takes NO voice read 2 % to 50 % depending on its notes and on the host seed
 * (the lead read 1 %), which no free-voice control can sit under. With blade 2 off
 * no line of the render draws, a chord on free voices reads 0.01 %, and the present
 * law still reads 12 % to 41 % at the taken chords. So the JUDGED rows are the pad
 * with blade 2 off, and the pad with it on is printed below as a NOTE, every run.
 *
 *   JUMP rows (judged). stealFade 1, each taken chord (3 to 6) at pool 6 and at pool
 *   8: the jump is at most kJumpCeil.
 *
 *   CONTROL rows (judged; LIBRARY L0032: a detector that cannot see the fault proves
 *   nothing). stealFade 0, the present law, the same renders: at least kControlMin
 *   at EVERY taken chord at pool 6, or this check is RED whatever the flag-on rows
 *   read. Pool 8 is printed and not required (one voice of three is taken at its
 *   third chord). The lead read 11 % to 19 % at pool 6 and 4 % to 10 % at pool 8,
 *   with other notes and blade 2 on; this check reads 12 % to 41 % and 4 % to 14 %.
 *
 *   FREE rows (judged; the must-read-zero control). The second chord, which finds
 *   three free voices: under kFreeCeil with the flag on and with it off.
 *
 *   LATE rows (judged). When the waiting notes start: the first sample at which the
 *   render with a taken chord differs from the same render with a panic sent right
 *   after the chord's note-ons. The panic cancels the waiting notes and lets their
 *   fades run out, and it changes nothing else here (every other voice was released
 *   0.1 s before), so the two are one render until the notes start. Required: that
 *   sample is kStealFade after the take, to the sample (240 at 48 kHz).
 *
 *   END rows (judged). No step where the fade ends: the largest |difference| between
 *   those two renders over the four samples from the start, as a share of the level.
 *   The old sound ends the same way in both, so this is what the starting notes add,
 *   and they start from silence under a 1.5 s attack: at most kEndCeil. Its detector
 *   is the JUMP's, whose fault control is CONTROL above; a start at the old
 *   envelope's level would read as the present law does there. (The render WITHOUT
 *   the chord cannot serve here: it still carries the tails the fades removed.)
 *
 *   THE POOL TAKEN WHOLE (added 2026-10-13, after an independent critic's read: the
 *   rows above take only the QUIETEST tails of a pool of 6 or 8, the easiest case).
 *   At a pool of 3 the second chord takes all three sounding voices, loud:
 *     tails  the first chord released 0.1 s before: three tails at about 0.8 of
 *            their level (tier 2);
 *     held   the first chord still held: three held voices (tier 3).
 *   Each is judged with blade 2 off AND with blade 2 on, on these rows:
 *     JUMP     as above, with a ceiling of its class (kWholeJumpCeil). It is larger
 *              than at pool 6 because ALL the sound is under the ramp: over four
 *              samples a 240-sample ramp removes up to 3/240 of it, 1.25 % of its
 *              instantaneous value, which stands above its rms.
 *     FADE     the fade itself (acceptance 1, "the old sound ends through a fade"):
 *              the peak of the output over the ramp's last tenth (24 samples), over
 *              the peak of the 50 ms before the take: at most kFadeCeil. Nothing else
 *              sounds there (every voice is under the ramp and the notes have not
 *              started), so this reads what is left of the old sound.
 *     NO-RAMP  FADE's must-fail, on every run (the critic deleted the ramp's scaling
 *              from the engine and this check stayed green: nothing read the fade).
 *              The same job through `control/steal-cut` (h2/harness/control_steal.h),
 *              a control adapter in the same binary that renders what an engine with
 *              no ramp renders: the old sound unfaded for the fade's length, then
 *              cut. Required: FADE reads OVER kFadeCeil on it, and the plant is what
 *              it claims (the chord-less render for the fade's length, this render
 *              after it, and not this render during it). Measured: the plant reads
 *              0.59 to 0.63; the engine with the ramp really deleted reads the same
 *              and turns FADE red (0.58 to 0.63).
 *     LATE, END  as above (END with the ceiling above).
 *   CONTROL, per class, blade 2 off: stealFade 0 reads at least kControlMin.
 *
 *   THE WHOLE RAMP, on the "tails" class (added 2026-10-13, after a second critic's
 *   read: JUMP reads the first 4 samples and FADE the last 24, and the 212 between
 *   were judged by nothing. An engine that cut the voice at the fifth sample, and one
 *   whose ramp stopped at 15/240 of the level, both read GREEN).
 *     RAMP     every sample of the fade against the gain it should have: over k = 0 ..
 *              N - 1 the largest |with[k] - ((N - k) / N) * without[k]|, on the
 *              pre-limiter bus (the harness's, atanh of the output), either channel,
 *              as a share of that bus's peak over the 50 ms before the take: at most
 *              kRampCeil. `without` is the render in which the old sound goes on
 *              (no chord). The tails are already releasing, so the take changes
 *              nothing of them but the ramp, and the two differ by the ramp alone.
 *              What is left (0.0016) is the memory of the oversampling's decimation
 *              filter, which the summed voices pass through after the ramp has scaled
 *              them (MEASURED on a plain patch in a scratch probe: 0.0019 at os 2,
 *              0.0000 at os 1; for the pad it is ARGUED from that, not measured apart).
 *
 *   THE PLANTS: WHICH FAULT EACH ROW IS THERE TO CATCH, AND ITS MUST-FAIL (LIBRARY
 *   L0032). Each is the same job through a control adapter in the same binary
 *   (h2/harness/control_steal.h, built from two shipped engines), read by the same
 *   measure, on every run; the check is red unless the row reads it as a fault AND the
 *   plant is, sample for sample, what it claims. Tails class, blade 2 off and on.
 *     row    the fault it catches             its must-fail             that plant reads
 *     JUMP   a ramp that is too fast          CONTROL (the present law: the fastest
 *            (the old sound gone at once)     there is)                 430 %
 *     FADE   no ramp                          NO-RAMP  steal-cut        0.60, 0.63
 *     RAMP   a cut inside the ramp;           CUT-IN   steal-cut5       0.71, 0.56
 *            a ramp that stops short          SHORT    steal-short      0.026, 0.023
 *     LATE   a start that is not N samples    EARLY    steal-early      239 (N - 1)
 *            after the take
 *     END    a start that steps (not from     STEP     steal-step       199 %, 196 %
 *            silence)
 *   CUT-IN prints what JUMP and FADE read on its plant, and SHORT what FADE reads: the
 *   readings of the real engine or better, which is why those two rows could not see
 *   them. On the critics' own mutants of the ENGINE (scratch builds, not in the tree;
 *   blade 2 off / on), this check reads:
 *     the ramp's scaling removed                    FADE 0.598 / 0.626, RAMP 0.513 / 0.534: RED
 *     right for 4 samples, the voice cut at the 5th RAMP 0.8141 / 0.6416: RED (JUMP and FADE green)
 *     the ramp stops at 15/240 and is cut at its end  RAMP 0.0255 / 0.0227: RED (JUMP and FADE green)
 *     a ramp ten times too fast                     JUMP 21.2 % / 18.1 % (ceiling 3 %), RAMP 0.28 / 0.29: RED
 *
 *     LATE at 44.1 kHz  the same take at 44 100 Hz: 0.005 s is 220.5 samples there, the
 *              engine rounds it to 221, and the notes must start 221 after the take.
 *
 *   HELD rows (judged; ADR-216 A3: THE TIERS JUDGE A VOICE BY ITS KEY, NOT BY ITS
 *   DELAYED GATE; added 2026-10-13 after the second critic's read). A chord of two
 *   held at a pool of 3 (attack 10 ms, release 100 ms), and a line of eight 0.1 s
 *   notes over it, each struck 0, 96 or 240 samples after the last one's release (0
 *   is what a sequencer sends). Each line note after the first takes the voice of
 *   the one before, which with the flag on is still sounding out its late release:
 *   its key is up, so it is a tail, and the chord is never touched. The reading is
 *   the level of the last quarter second, a second after the line's last release,
 *   against the same render with no line: within kHeldDb. As first built the late
 *   release read as HELD, the line took a chord voice from its third note on, and
 *   this row reads -2.4 dB at 0 and at 96 samples (measured on a scratch build with
 *   that put back). With the flag off it reads the same as with it on.
 *   LOST     its must-fail, every run: the same line at a pool of 2, which the chord
 *            fills, so the line's first note MUST take a chord voice. Required: at
 *            least kHeldLossDb under the chord alone.
 *
 *   WHICH ROWS NEED BLADE 2 OFF, AND WHICH DO NOT. A comparison of two renders is
 *   confounded only if one of them draws from the shared stream where the other
 *   does not WHILE a noise blade sounds. A take draws nothing at the note-on (the
 *   note waits), and its start draws when the fade ends. So:
 *     - FREE and every CONTROL need it off: a note on a free voice and a flag-off
 *       restart both draw at the note-on.
 *     - JUMP with the flag on does not, where every note of the chord waits: the
 *       pool taken whole is judged with blade 2 on too. (At pool 6 and 8 the rows
 *       stay on the blade-2-off basis they were measured on; their blade-2-on
 *       readings are the NOTE lines. At pool 8 the third chord finds two free
 *       voices, which draw.)
 *     - FADE reads one render: no confound.
 *     - LATE and END compare with the cancelled twin, which never draws: sound at
 *       the pool taken whole, where no other voice sounds when the notes start;
 *       at pool 6 and 8 other tails sound, so blade 2 is off there.
 *
 *   BLOCKS rows (judged). Host blocks of 16, 37 and 128 frames, flag on and off:
 *   with the flag on the three renders must agree at least as well as they do with
 *   it off (the largest |difference| from the 128-frame render). Two scripts: the
 *   six chords at pool 6; and four chords at a pool of 3 with `keepPhase` 1 and
 *   random start phases, blade 2 on, where each chord takes the whole pool. The
 *   second is the case the critic found: with nothing else sounding when a fade
 *   ends, the keep-phase snapshot was the one left at the host's last block
 *   boundary (1.08 and 1.01 at 16 and 37 frames with that fault put back). The
 *   fade's end falls inside a host block at 37 and at 128.
 *   A third script, required BIT-IDENTICAL outright with the flag on (the second
 *   critic's case): `keepPhase` 1, a pool of 1 and a release of 5 ms. A tail taken
 *   300 samples after its release, just above the threshold, dies of its own release
 *   floor under the ramp; the take is cancelled by a panic; a note is struck 200
 *   samples into the fade and restarts the silent voice at once. The faded voice must
 *   count as sounding for the whole fade (engine.h renderSpan), or that note reads
 *   the snapshot of the last host block the voice was heard in: 0.60 at 16 frames and 0.42 at 37
 *   with that fault put back (scratch build). The flag-off render of the script is
 *   printed and not judged.
 *   WHAT THESE ROWS DO NOT CLAIM. With `keepPhase` on the engine has a block-size
 *   dependence of its own, older than D7 and present with the flag off: a voice with
 *   NO fade on it that dies of its release floor when nothing else sounds leaves the
 *   snapshot of the last host block it was heard in. A flag-on render that contains
 *   such a death carries it too. D7 adds none (the B459 trace has the fuzz).
 *
 *   MONO rows (judged). Mono and legato are untouched: an overlapping phrase with
 *   `polyMode` 1 and with 2 renders the same samples with the flag on and off.
 *
 *   LIVENESS rows (judged). A silent render reads no jump. Each flag-on six-chord
 *   render is audible and within kLiveDb of its flag-off twin in rms.
 *
 *   NOTE lines (printed, NOT judged). The pad with blade 2 ON (the seven pins only,
 *   the settings the lead measured under), and that with the patch's own spread law
 *   on as well (`law` 4), chords 2 to 4 at pool 6, flag off and on. Both carry the
 *   shared-stream confound: the lead measured about 4 % at a chord that takes no
 *   voice under law 4. They are printed so the readings stay on the record.
 *
 * THE CEILINGS ARE MEASURED, AND PROPOSED FOR THE HUMAN TO RATIFY (the human's
 * standing rule: measured worst plus a stated margin, ADR-209 item 5). Measured
 * 2026-10-13 on darwin-arm64:
 *   kJumpCeil   0.5 %, for a take of the quietest tails at pool 6 and 8. Worst
 *               0.227 % (pool 6, chord 3); the margin is about twice that. What it
 *               bounds: over four samples a 240-sample ramp removes at most 3/240
 *               of the TAKEN voices' sound, and there they are a small share of it.
 *   kWholeJumpCeil  for a pool taken whole. tails: 3 %, worst 2.073 % (blade 2
 *               off; 1.793 % on), about one and a half times. held: 1.5 %, worst
 *               0.794 % (blade 2 off; 0.550 % on), about twice.
 *   kFadeCeil   0.08. Worst 0.041 (held, blade 2 off; the four read 0.032 to 0.041);
 *               about twice. The ramp's own gain over its last tenth is under 0.1.
 *   kEndCeil    0.05 %. Worst 0.016 % (pool 8, chord 5; 0.010 % at the pool taken
 *               whole); about three times that.
 *   kRampCeil   0.005. Worst 0.0016 (tails, blade 2 off and on); about three times
 *               that. The weakest plant it must see, the ramp that stops short,
 *               reads 0.0227: four and a half times the ceiling.
 * TWO BOUNDS THAT ARE NOT CEILINGS OF THE FADE, also PROPOSED: kHeldDb 0.25 dB (the
 * held chord after the line: the worst measured is 0.004 dB, and a lost chord note
 * reads 2.4 dB) and kHeldLossDb 1 dB (what the LOST control must read at least).
 * NOT ceilings of mine: kControlMin (5 %) and kFreeCeil (1 %) are the brief's, from
 * ROADMAP B459's acceptance.
 *
 * ALSO REQUIRED OF EVERY JOB: the engine knew every key (a misspelt `stealFade`
 * would render the present law twice).
 *
 * THE LAST LINE IS THE VERDICT: `h2_steal_fade_check: GREEN (...)` or `RED (...)`.
 * Exit 0 green, 1 red, 2 the run broke (no binary, a refused job).
 *
 *   node tools/h2_steal_fade_check.mjs [--bin <h2_harness_render>]
 *
 * Deterministic: no clock, no randomness but the engine's seeded streams.
 */
import { realpathSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import { jobFor, mtof, patch, patchCmds, run } from '../h2/harness/harness.mjs';

const SR = 48000, SEED = 1, VEL = 0.85;
const PAD = 'ledger/breathing-pad';
// The seven settings the lead measured under (header), and the eighth this check adds.
export const kLeadPinned = { detune: 14, K: 0.45, law: 0, A: 1500, S: 0.9, R: 3000, N: 6 };
export const kJudgedPinned = { ...kLeadPinned, b2on: 0 };
// C, F, G, A minor, E minor, D minor, rooted at C3 and below C4: six different triads,
// so no chord repeats a note of the chord whose tails it takes.
const tri = (root, minor) => [root, root + (minor ? 3 : 4), root + 7];
export const kChords = [tri(48), tri(53), tri(55), tri(57, true), tri(52, true), tri(50, true)];
const kHold = Math.round(1.6 * SR), kGap = Math.round(0.1 * SR);
const kLevelWindow = Math.round(0.05 * SR), kJumpSamples = 4;
const kPools = [6, 8];
const kFreeChord = 2, kTakenChords = [3, 4, 5, 6];   // 1-based, as the row counts them
// engine.h kStealFade, in samples as the engine converts it (round to nearest)
const kStealFade = 0.005, kFadeSamples = Math.max(1, Math.round(kStealFade * SR));
const kWindow = kFadeSamples + 64;   // rendered after a strike: the fade, the start, and room
// the bounds (header)
const kJumpCeil = 0.005, kEndCeil = 0.0005;   // PROPOSED: measured worst plus margin
// ... and for a pool taken whole (PROPOSED the same way; header, "THE CEILINGS")
const kWholeJumpCeil = { tails: 0.03, held: 0.015 }, kWholeEndCeil = kEndCeil, kFadeCeil = 0.08, kRampCeil = 0.005;
// the held chord under a line (header, HELD): how near the chord alone it must read, and how far under a lost note must
const kHeldDb = 0.25, kHeldLossDb = 1;
const kControlMin = 0.05, kFreeCeil = 0.01;   // ROADMAP B459's acceptance
// Liveness: measured 2026-10-13, the flag-on six-chord renders are 0.30 dB (pool 6)
// and 0.12 dB (pool 8) under their flag-off twins: a restarted voice attacks from its
// old level, a waiting note from silence. 3 dB is tools/h2_frame_hold_check.mjs's
// bound, about ten times the larger, and far under a render that lost its notes.
const kLiveDb = 3;
const kSilentPeak = Math.pow(10, -90 / 20);   // tools/patchspace/metrics.mjs's silence line, -90 dBFS peak
const kBlocks = [128, 16, 37];                // the first is the one the others are compared with
const kNoteChords = [2, 3, 4];                // the unjudged lines stop at the fourth chord (cost)

/* ---- the scripts. T(k) is the frame the k-th chord (1-based) is struck at. */
const T = k => (k - 1) * (kHold + kGap);
const strike = c => c.map(n => ['on', n, mtof(n), VEL]);
const lift = c => c.map(n => ['off', n]);
const head = (pinned, pool, flag) => [...patchCmds(patch(PAD), { ...pinned, stealFade: flag }), ['set', 'poly', pool]];
/* chords 1..n, each held and released, each followed by its gap */
const played = n => { const c = []; for (let i = 0; i < n; i++) c.push(...strike(kChords[i]), ['render', kHold], ...lift(kChords[i]), ['render', kGap]); return c; };
/* every chord up to `last`, the last one held to the end: chord k's strike is at T(k) */
const whole = (pinned, pool, flag, last = kChords.length) =>
  [...head(pinned, pool, flag), ...played(last - 1), ...strike(kChords[last - 1]), ['render', kHold]];
/* the history before chord k, then NOT the chord */
const without = (pinned, pool, flag, k) => [...head(pinned, pool, flag), ...played(k - 1), ['render', kWindow]];
/* the history, the chord, and a panic at once: its waiting notes are cancelled */
const cancelled = (pinned, pool, flag, k) => [...head(pinned, pool, flag), ...played(k - 1), ...strike(kChords[k - 1]), ['panic'], ['render', kWindow]];
const monoPhrase = (mode, flag) => [...patchCmds(patch(PAD), { ...kJudgedPinned, polyMode: mode, stealFade: flag }),
  ['on', 48, mtof(48), VEL], ['render', 14400], ['on', 55, mtof(55), VEL], ['render', 14400], ['on', 60, mtof(60), VEL], ['render', 14400],
  ['off', 60], ['render', 9600], ['off', 55], ['off', 48], ['render', 14400]];

/* ---- the measurements */
/* largest |a - b| on either channel over frames [fromA, fromA + n) of a and [fromB, ...) of b */
function maxDiff(a, fromA, b, fromB, n) {
  let m = 0;
  for (let i = 0; i < n; i++) {
    const l = Math.abs(a.L[fromA + i] - b.L[fromB + i]), r = Math.abs(a.R[fromA + i] - b.R[fromB + i]);
    if (!(l <= m)) m = l;   // a NaN difference is kept, and fails every bound
    if (!(r <= m)) m = r;
  }
  return m;
}
/* the first i in [0, n) at which the two differ, or -1 */
function firstDiff(a, fromA, b, fromB, n) {
  for (let i = 0; i < n; i++) if (a.L[fromA + i] !== b.L[fromB + i] || a.R[fromA + i] !== b.R[fromB + i]) return i;
  return -1;
}
/* rms of both channels over [from, to), and the peak */
function level(r, from = 0, to = r.L.length) {
  let e = 0, pk = 0;
  for (let i = from; i < to; i++) { e += r.L[i] * r.L[i] + r.R[i] * r.R[i]; pk = Math.max(pk, Math.abs(r.L[i]), Math.abs(r.R[i])); }
  return { rms: Math.sqrt(e / (2 * (to - from))), peak: pk };
}
const pct = (x, d = 2) => `${(100 * x).toFixed(d)} %`;
// The chords whose three notes ALL wait (LATE and END need that: a note that finds a
// free voice sounds at once, and the panic would silence it). At pool 8 the third
// chord finds two free voices.
const kAllWait = { 6: [3, 4, 5, 6], 8: [4, 5, 6] };

/* ---- a pool taken whole (header, "THE POOL TAKEN WHOLE") */
const kWholePool = 3;
const kClasses = [
  { id: 'tails', what: 'three releasing tails, 0.1 s after their release (tier 2)', t: kHold + kGap,
    hist: () => [...strike(kChords[0]), ['render', kHold], ...lift(kChords[0]), ['render', kGap]] },
  { id: 'held', what: 'three held voices (tier 3)', t: kHold,
    hist: () => [...strike(kChords[0]), ['render', kHold]] },
];
const kBases = [['off', 'blade 2 off', kJudgedPinned], ['on', 'blade 2 ON', kLeadPinned]];
/* the class's history, then the second chord (kind 'with'), or not ('without'), or the chord and a panic ('cancel') */
const classJob = (pinned, flag, c, kind) => [...head(pinned, kWholePool, flag), ...c.hist(),
  ...(kind === 'without' ? [] : strike(kChords[1])), ...(kind === 'cancel' ? [['panic']] : []), ['render', kWindow]];
const kFadeTail = Math.round(kFadeSamples / 10);   // the fade's last tenth, in samples
// h2/harness/control_steal.h: the planted faults, and the row each must be read by (header, "THE PLANTS")
const kPlants = { cut: 'control/steal-cut', cut5: 'control/steal-cut5', short: 'control/steal-short', step: 'control/steal-step', early: 'control/steal-early' };
const kCutAt = 4, kShortLeft = 15;                 // control_steal.h's kCutAt and kShortLeft
const kPlantClass = 'tails';                       // the class the whole-ramp row and the four later plants are played on
const SR441 = 44100, kFadeSamples441 = Math.max(1, Math.round(kStealFade * SR441));
/* keep-phase at a pool the chords take whole: four triads, each held and released */
const kKeepPinned = { ...kLeadPinned, keepPhase: 1, phaseMode: 0 };
const keepJob = flag => [...head(kKeepPinned, kWholePool, flag), ...played(4)];
/* keep-phase and a voice that DIES UNDER ITS RAMP (header, BLOCKS): a pool of 1, a release of 5 ms. The note is
   released, taken kDieTake samples later while it is just above the threshold, and the take is cancelled by a
   panic; the next note is struck kDieNext samples into the fade, after the voice's own release floor ended it */
const kDiePinned = { ...kLeadPinned, keepPhase: 1, phaseMode: 0, A: 5, R: 5 };
const kDieTake = 300, kDieNext = 200;
const dieJob = flag => [...head(kDiePinned, 1, flag), ['on', 48, mtof(48), VEL], ['render', 4800], ['off', 48], ['render', kDieTake],
  ['on', 53, mtof(53), VEL], ['panic'], ['render', kDieNext], ['on', 57, mtof(57), VEL], ['render', 9600]];
/* a held chord under a back-to-back line (header, HELD): the chord's two notes held throughout; eight line notes,
   each struck `gap` samples after the last one's release (0: at the same instant, as a sequencer sends them);
   then a second of the chord alone. `line` false: the chord and no line, the same length. */
const kHeldPinned = { ...kJudgedPinned, A: 10, R: 100 };
const kHeldChord = [48, 55], kLine = [72, 74, 76, 77, 79, 77, 76, 74];
const kLineNote = Math.round(0.1 * SR), kHeldLead = Math.round(0.4 * SR), kHeldTail = SR, kHeldRead = SR / 4;
const kHeldGaps = [0, 96, 240], kHeldPool = 3, kHeldLossPool = 2;
const heldJob = (pool, flag, gap, line) => {
  const c = [...head(kHeldPinned, pool, flag), ...strike(kHeldChord), ['render', kHeldLead]];
  if (!line) return [...c, ['render', kLine.length * kLineNote + kHeldTail]];
  kLine.forEach((n, i) => {
    if (i) { c.push(['off', kLine[i - 1]]); if (gap) c.push(['render', gap]); }
    c.push(['on', n, mtof(n), VEL], ['render', kLineNote - (i ? gap : 0)]);
  });
  return [...c, ['off', kLine[kLine.length - 1]], ['render', kHeldTail]];
};

export function main(argv) {
  const bi = argv.indexOf('--bin'), opt = bi >= 0 ? { bin: argv[bi + 1] } : {};
  let red = 0;
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(8)} ${text}`); if (!ok) red++; };
  const note = (tag, text) => console.log(`NOTE  ${tag.padEnd(8)} ${text}`);
  patch(PAD);   // throws if the pad has left the reference set

  // One process renders everything.
  const jobs = [], at = {};
  // o.engine: a control adapter in the composed engine's place; o.pre: the pre-limiter bus too; o.sr: another rate
  const add = (key, cmds, block, o = {}) => {
    at[key] = jobs.length;
    jobs.push(jobFor({ ...(o.engine ? { engine: o.engine } : { patch: PAD }), sr: o.sr || SR, seed: SEED, block, cmds, pre: !!o.pre, name: key }));
  };
  for (const pool of kPools) for (const fl of [1, 0]) {
    for (const b of pool === 6 ? kBlocks : [kBlocks[0]]) add(`J/${pool}/${fl}/whole/${b}`, whole(kJudgedPinned, pool, fl), b);
    for (const k of [kFreeChord, ...kTakenChords]) add(`J/${pool}/${fl}/without/${k}`, without(kJudgedPinned, pool, fl, k));
  }
  for (const pool of kPools) for (const k of kAllWait[pool]) add(`J/${pool}/cancel/${k}`, cancelled(kJudgedPinned, pool, 1, k));
  for (const mode of [1, 2]) for (const fl of [1, 0]) add(`M/${mode}/${fl}`, monoPhrase(mode, fl));
  // a pool taken whole: per class and basis, with the flag on: the chord, no chord, the
  // chord cancelled, and the chord through the plants (no ramp on both classes; the
  // other four on kPlantClass); with the flag off (blade 2 off only: a restart
  // draws): the chord and no chord
  for (const c of kClasses) for (const [b, , pinned] of kBases) {
    for (const kind of ['with', 'without', 'cancel']) add(`W/${c.id}/${b}/1/${kind}`, classJob(pinned, 1, c, kind), undefined, { pre: true });
    for (const [pid, engine] of Object.entries(kPlants)) if (pid === 'cut' || c.id === kPlantClass)
      add(`W/${c.id}/${b}/plant/${pid}`, classJob(pinned, 1, c, 'with'), undefined, { engine, pre: true });
    if (b === 'off') for (const kind of ['with', 'without']) add(`W/${c.id}/${b}/0/${kind}`, classJob(pinned, 0, c, kind));
  }
  // ... and the same take at 44.1 kHz, for the start's lateness there
  for (const kind of ['with', 'cancel']) add(`W441/${kind}`, classJob(kJudgedPinned, 1, kClasses[0], kind), undefined, { sr: SR441 });
  for (const fl of [1, 0]) for (const b of kBlocks) add(`K/${fl}/${b}`, keepJob(fl), b);
  for (const fl of [1, 0]) for (const b of kBlocks) add(`D/${fl}/${b}`, dieJob(fl), b);
  // a held chord under a line: the pool the chord leaves a voice in, flag on at each gap and
  // flag off at the first; the chord alone; and the pool the chord fills (the loss control)
  for (const gap of kHeldGaps) add(`H/${kHeldPool}/1/${gap}`, heldJob(kHeldPool, 1, gap, true));
  add(`H/${kHeldPool}/0/${kHeldGaps[0]}`, heldJob(kHeldPool, 0, kHeldGaps[0], true));
  add(`H/${kHeldLossPool}/1/${kHeldGaps[0]}`, heldJob(kHeldLossPool, 1, kHeldGaps[0], true));
  for (const pool of [kHeldPool, kHeldLossPool]) add(`H/${pool}/chord`, heldJob(pool, 1, 0, false));
  // not judged: blade 2 on (the lead's seven pins), and that with spread law 4
  const NOTES = [['blade2', 'blade 2 ON, spread law off', kLeadPinned], ['law4', 'blade 2 ON, spread law 4', { ...kLeadPinned, law: 4 }]];
  const lastNote = Math.max(...kNoteChords);
  for (const [id, , pinned] of NOTES) for (const fl of [1, 0]) {
    add(`N/${id}/${fl}/whole`, whole(pinned, 6, fl, lastNote));
    for (const k of kNoteChords) add(`N/${id}/${fl}/without/${k}`, without(pinned, 6, fl, k));
  }
  const rs = run(jobs, opt), R = key => rs[at[key]];
  const unknown = rs.filter(r => r.summary.unknownKeys);
  row(!unknown.length, 'KEYS', `the engine knew every key on ${rs.length - unknown.length} of ${rs.length} jobs${unknown.length ? `; first refused on: ${unknown[0].name}` : ''}`);

  /* the jump at chord k: the whole render against the one without the chord */
  const jump = (prefix, k) => {
    const w = R(`${prefix}/whole${prefix.startsWith('J/') ? '/' + kBlocks[0] : ''}`), wo = R(`${prefix}/without/${k}`), t = T(k);
    return maxDiff(w, t, wo, t, kJumpSamples) / level(w, t - kLevelWindow, t).rms;
  };
  console.log(`judged: ${PAD} with ${JSON.stringify(kJudgedPinned)}; ${kChords.length} triads ${JSON.stringify(kChords)} at velocity ${VEL}, held ${kHold / SR} s, struck ${kGap / SR} s after the last release; ${SR} Hz, seed ${SEED}; ` +
    `the jump is over ${kJumpSamples} samples, against the rms of the ${1000 * kLevelWindow / SR} ms before`);

  // ---- the jump, flag on (judged) and flag off (its control)
  let worstOn = 0, worstOnAt = '';
  for (const pool of kPools) {
    for (const k of kTakenChords) {
      const on = jump(`J/${pool}/1`, k), off = jump(`J/${pool}/0`, k);
      if (!(on <= worstOn)) { worstOn = on; worstOnAt = `pool ${pool}, chord ${k}`; }
      row(on <= kJumpCeil, 'JUMP', `pool ${pool}, chord ${k}, stealFade 1: ${pct(on, 3)}; ceiling ${pct(kJumpCeil, 1)} (PROPOSED)`);
      const must = pool === 6;
      row(!must || off >= kControlMin, 'CONTROL', `pool ${pool}, chord ${k}, stealFade 0 (the present law): ${pct(off)}; ${must ? `required at least ${pct(kControlMin, 0)}` : 'not required'}`);
    }
  }
  row(worstOn <= kJumpCeil, 'JUMP', `worst with stealFade 1: ${pct(worstOn, 3)} (${worstOnAt}); ceiling ${pct(kJumpCeil, 1)}, PROPOSED for the human: the worst measured 2026-10-13 was 0.227 %, and the margin is about twice that`);

  // ---- the free-voice control: no voice is taken, so there is nothing to read
  for (const pool of kPools) for (const fl of [1, 0]) {
    const j = jump(`J/${pool}/${fl}`, kFreeChord);
    row(j < kFreeCeil, 'FREE', `pool ${pool}, chord ${kFreeChord} (three free voices), stealFade ${fl}: ${pct(j, 3)}; required under ${pct(kFreeCeil, 0)}`);
  }

  // ---- when the waiting notes start, and what the start adds
  let worstEnd = 0, worstEndAt = '';
  for (const pool of kPools) for (const k of kAllWait[pool]) {
    const w = R(`J/${pool}/1/whole/${kBlocks[0]}`), c = R(`J/${pool}/cancel/${k}`), t = T(k);
    const first = firstDiff(w, t, c, t, kWindow);
    row(first === kFadeSamples, 'LATE', `pool ${pool}, chord ${k}: its notes start ${first < 0 ? 'NOWHERE in the window' : `${first} samples (${(1000 * first / SR).toFixed(3)} ms)`} after the take; ` +
      `required ${kFadeSamples} samples, ${1000 * kStealFade} ms at ${SR} Hz, to the sample`);
    if (first < 0) continue;
    const e = maxDiff(w, t + first, c, t + first, kJumpSamples) / level(w, t - kLevelWindow, t).rms;
    if (!(e <= worstEnd)) { worstEnd = e; worstEndAt = `pool ${pool}, chord ${k}`; }
    row(e <= kEndCeil, 'END', `pool ${pool}, chord ${k}: over the ${kJumpSamples} samples from the start, the starting notes add ${pct(e, 4)}; ceiling ${pct(kEndCeil, 2)} (PROPOSED)`);
  }
  row(worstEnd <= kEndCeil, 'END', `worst: ${pct(worstEnd, 4)} (${worstEndAt}); ceiling ${pct(kEndCeil, 2)}, PROPOSED for the human: the worst measured 2026-10-13 was 0.016 %, and the margin is about three times that`);

  // ---- a pool taken whole: the jump, the fade itself, the start (header)
  const peak = (r, from, to) => level(r, from, to).peak;
  for (const c of kClasses) {
    console.log(`pool ${kWholePool} taken whole, ${c.id}: ${c.what}; chord ${JSON.stringify(kChords[1])} struck over ${JSON.stringify(kChords[0])}`);
    for (const [b, bName] of kBases) {
      const w = R(`W/${c.id}/${b}/1/with`), wo = R(`W/${c.id}/${b}/1/without`), cn = R(`W/${c.id}/${b}/1/cancel`), t = c.t;
      const plant = pid => R(`W/${c.id}/${b}/plant/${pid}`), pl = plant('cut');
      const lv = level(w, t - kLevelWindow, t), end = t + kFadeSamples, tail = r => peak(r, end - kFadeTail, end) / lv.peak;
      const j = maxDiff(w, t, wo, t, kJumpSamples) / lv.rms;
      row(j <= kWholeJumpCeil[c.id], 'JUMP', `${c.id}, ${bName}, stealFade 1: ${pct(j, 3)}; ceiling ${pct(kWholeJumpCeil[c.id], 1)} (PROPOSED)`);
      // the fade itself: what is left of the old sound in the ramp's last tenth
      const f = tail(w), fp = tail(pl);
      row(f <= kFadeCeil, 'FADE', `${c.id}, ${bName}: the peak over the fade's last ${kFadeTail} samples is ${f.toFixed(3)} of the peak before the take; ceiling ${kFadeCeil} (PROPOSED)`);
      // ... and its must-fail: the same job through the no-ramp plant, which must be what it
      // claims (the chord-less render for the fade's length, then this render) and must read over the ceiling
      const after = r => firstDiff(r, end, w, end, kWindow - kFadeSamples) < 0;   // the plant hands back to the engine's own render
      const isPlant = firstDiff(pl, t, wo, t, kFadeSamples) < 0 && after(pl) && firstDiff(pl, t, w, t, kFadeSamples) >= 0;
      row(isPlant && fp > kFadeCeil, 'NO-RAMP', `CONTROL ${c.id}, ${bName}: the same job through ${kPlants.cut} (the old sound unfaded for ${kFadeSamples} samples, then cut) reads ${fp.toFixed(3)}; ` +
        `required over ${kFadeCeil}, and the plant ${isPlant ? 'is' : 'IS NOT'} the chord-less render for the fade's length and this render after it`);
      const first = firstDiff(w, t, cn, t, kWindow);
      row(first === kFadeSamples, 'LATE', `${c.id}, ${bName}: its notes start ${first < 0 ? 'NOWHERE in the window' : `${first} samples (${(1000 * first / SR).toFixed(3)} ms)`} after the take; required ${kFadeSamples}`);
      const endOf = (r, from) => maxDiff(r, t + from, cn, t + from, kJumpSamples) / lv.rms;
      if (first >= 0) {
        const e = endOf(w, first);
        row(e <= kWholeEndCeil, 'END', `${c.id}, ${bName}: over the ${kJumpSamples} samples from the start, the starting notes add ${pct(e, 4)}; ceiling ${pct(kWholeEndCeil, 2)} (PROPOSED)`);
      }
      if (c.id !== kPlantClass) continue;

      // THE WHOLE RAMP (header): every sample of the fade against the gain it should have,
      // on the pre-limiter bus, as a share of the peak before the take
      const N = kFadeSamples, prePeak = Math.max(...[w.preL, w.preR].map(a => { let m = 0; for (let i = t - kLevelWindow; i < t; i++) m = Math.max(m, Math.abs(a[i])); return m; }));
      const ramp = r => {
        let m = 0;
        for (let k = 0; k < N; k++) for (const [a, o] of [[r.preL, wo.preL], [r.preR, wo.preR]]) {
          const d = Math.abs(a[t + k] - ((N - k) / N) * o[t + k]);
          if (!(d <= m)) m = d;   // a NaN or an infinity (the limiter's rail) is kept, and fails the bound
        }
        return m / prePeak;
      };
      const rw = ramp(w);
      row(rw <= kRampCeil, 'RAMP', `${c.id}, ${bName}: over the fade's ${N} samples the output is within ${rw.toFixed(4)} of (the gain it should have) x (the old sound going on), as a share of the peak before the take; ceiling ${kRampCeil} (PROPOSED)`);
      // ... and its two must-fails, each of which the rows above are blind to
      const p5 = plant('cut5'), ps = plant('short');
      const zero = (r, from, to) => { for (let i = from; i < to; i++) if (r.L[i] !== 0 || r.R[i] !== 0) return false; return true; };
      const is5 = firstDiff(p5, t, w, t, kCutAt) < 0 && zero(p5, t + kCutAt, end) && after(p5);
      const j5 = maxDiff(p5, t, wo, t, kJumpSamples) / lv.rms, f5 = tail(p5), r5 = ramp(p5);
      row(is5 && r5 > kRampCeil, 'CUT-IN', `CONTROL ${c.id}, ${bName}: the same job through ${kPlants.cut5} (the ramp right for ${kCutAt} samples, then nothing) reads ${r5.toFixed(4)} on RAMP; required over ${kRampCeil}, ` +
        `and the plant ${is5 ? 'is' : 'IS NOT'} this render for ${kCutAt} samples, zero to the fade's end and this render after it. JUMP reads it ${pct(j5, 3)} and FADE ${f5.toFixed(3)}: neither can see it`);
      const kept = N - kShortLeft + 1;   // samples 0 .. N - kShortLeft are the engine's own: its gain there is at least kShortLeft / N
      const isS = firstDiff(ps, t, w, t, kept) < 0 && firstDiff(ps, t + kept, w, t + kept, kShortLeft - 1) >= 0 && after(ps);
      const fS = tail(ps), rS = ramp(ps);
      row(isS && rS > kRampCeil, 'SHORT', `CONTROL ${c.id}, ${bName}: the same job through ${kPlants.short} (the ramp stops at ${kShortLeft}/${N} of the level, then a cut) reads ${rS.toFixed(4)} on RAMP; required over ${kRampCeil}, ` +
        `and the plant ${isS ? 'is' : 'IS NOT'} this render until the ramp's gain is ${kShortLeft}/${N}, another after, and this render from the fade's end. FADE reads it ${fS.toFixed(3)}${fS <= kFadeCeil ? ': it cannot see it' : ''}`);
      note('RAMP', `${c.id}, ${bName}: ${kPlants.cut} (no ramp) reads ${ramp(pl).toFixed(4)} on RAMP (NOT judged here; FADE's control is NO-RAMP)`);
      // LATE's and END's must-fails: a start one sample early, and a start on time that steps
      const pe = plant('early'), pstep = plant('step');
      const firstE = firstDiff(pe, t, cn, t, kWindow), isE = firstDiff(pe, t, w, t, N - 1) < 0;
      row(isE && firstE === N - 1, 'EARLY', `CONTROL ${c.id}, ${bName}: through ${kPlants.early} (the notes start one sample before the fade's end) LATE reads ${firstE}; required ${N - 1}, which LATE fails, ` +
        `and the plant ${isE ? 'is' : 'IS NOT'} this render for the ${N - 1} samples before`);
      const firstS = firstDiff(pstep, t, cn, t, kWindow), isSt = firstDiff(pstep, t, w, t, N) < 0, eS = firstS >= 0 ? endOf(pstep, firstS) : NaN;
      row(isSt && firstS === N && eS > kWholeEndCeil, 'STEP', `CONTROL ${c.id}, ${bName}: through ${kPlants.step} (the notes start on time, at the old envelope's level) LATE reads ${firstS} (required ${N}: on time) and END reads ${pct(eS, 2)}; ` +
        `required over ${pct(kWholeEndCeil, 2)}, and the plant ${isSt ? 'is' : 'IS NOT'} this render for the fade's length`);
    }
    const w0 = R(`W/${c.id}/off/0/with`), wo0 = R(`W/${c.id}/off/0/without`);
    const j0 = maxDiff(w0, c.t, wo0, c.t, kJumpSamples) / level(w0, c.t - kLevelWindow, c.t).rms;
    row(j0 >= kControlMin, 'CONTROL', `${c.id}, blade 2 off, stealFade 0 (the present law): ${pct(j0)}; required at least ${pct(kControlMin, 0)}`);
  }
  // ... and the start's lateness at 44.1 kHz, where 0.005 s is 220.5 samples and the engine rounds it up
  {
    const c = kClasses[0], first = firstDiff(R('W441/with'), c.t, R('W441/cancel'), c.t, kWindow);
    row(first === kFadeSamples441, 'LATE', `${c.id}, blade 2 off, at ${SR441} Hz: its notes start ${first < 0 ? 'NOWHERE in the window' : `${first} samples (${(1000 * first / SR441).toFixed(3)} ms)`} after the take; required ${kFadeSamples441}`);
  }

  // ---- a held chord under a back-to-back line (ADR-216 A3: the tiers judge a voice by its key)
  {
    const tailDb = (r, ref) => { const n = r.L.length; return 20 * Math.log10(level(r, n - kHeldRead, n).rms / level(ref, n - kHeldRead, n).rms); };
    const alone = R(`H/${kHeldPool}/chord`), lone = level(alone, alone.L.length - kHeldRead, alone.L.length);
    console.log(`a held chord ${JSON.stringify(kHeldChord)} under a line ${JSON.stringify(kLine)} of ${kLineNote / SR} s notes, ${JSON.stringify(kHeldPinned)}: the level of the last ${kHeldRead / SR} s, ` +
      `${kHeldTail / SR} s after the line's last release, against the chord alone (rms ${lone.rms.toFixed(4)})`);
    for (const [fl, gap] of [...kHeldGaps.map(g => [1, g]), [0, kHeldGaps[0]]]) {
      const db = tailDb(R(`H/${kHeldPool}/${fl}/${gap}`), alone);
      row(lone.peak >= kSilentPeak && Math.abs(db) <= kHeldDb, 'HELD', `pool ${kHeldPool}, stealFade ${fl}, each line note struck ${gap} samples after the last one's release: the chord reads ${db.toFixed(3)} dB against the chord alone; required within ${kHeldDb} dB`);
    }
    const lossDb = tailDb(R(`H/${kHeldLossPool}/1/${kHeldGaps[0]}`), R(`H/${kHeldLossPool}/chord`));
    row(lossDb <= -kHeldLossDb, 'LOST', `CONTROL pool ${kHeldLossPool} (the chord fills it, so the line must take a chord voice), stealFade 1: the chord reads ${lossDb.toFixed(3)} dB against the chord alone; required at least ${kHeldLossDb} dB under`);
  }

  // ---- the host's block size
  const spread = key => fl => {
    const ref = R(key(fl, kBlocks[0]));
    return kBlocks.slice(1).map(b => { const r = R(key(fl, b)); return { b, d: maxDiff(ref, 0, r, 0, ref.L.length), same: r.summary.digest === ref.summary.digest }; });
  };
  const worstB = s => s.reduce((m, x) => (x.d <= m ? m : x.d), 0);
  const tell = s => s.map(x => `${x.b} frames ${x.d === 0 ? 'bit-identical' : x.d.toExponential(2)}${x.same === (x.d === 0) ? '' : ' (DIGEST DISAGREES)'}`).join(', ');
  for (const [what, key] of [['the six chords at pool 6, blade 2 off', (fl, b) => `J/6/${fl}/whole/${b}`],
    [`four chords at pool ${kWholePool} (taken whole) with keepPhase 1 and random start phases, blade 2 ON`, (fl, b) => `K/${fl}/${b}`]]) {
    const bOn = spread(key)(1), bOff = spread(key)(0);
    row(worstB(bOn) <= worstB(bOff) && [...bOn, ...bOff].every(x => x.same === (x.d === 0)), 'BLOCKS',
      `${what}, largest |difference| from the ${kBlocks[0]}-frame render: stealFade 1: ${tell(bOn)}; stealFade 0: ${tell(bOff)}; required: the flag adds none`);
  }
  // a voice that dies under its ramp, and a note struck before the ramp ends: required bit-identical outright
  {
    const key = (fl, b) => `D/${fl}/${b}`, bOn = spread(key)(1), bOff = spread(key)(0), lv = level(R(key(1, kBlocks[0])), 4800 + kDieTake + kDieNext);
    row(worstB(bOn) === 0 && bOn.every(x => x.same) && lv.peak >= kSilentPeak, 'BLOCKS',
      `keepPhase 1, a pool of 1, release 5 ms: a tail taken ${kDieTake} samples after its release dies under its ramp, the take is cancelled, and a note is struck ${kDieNext} samples into the fade (peak after ${lv.peak.toFixed(3)}); ` +
      `largest |difference| from the ${kBlocks[0]}-frame render: stealFade 1: ${tell(bOn)}; required bit-identical. (stealFade 0, NOT judged: ${tell(bOff)})`);
  }

  // ---- mono and legato
  for (const mode of [1, 2]) {
    const a = R(`M/${mode}/1`), b = R(`M/${mode}/0`), d = maxDiff(a, 0, b, 0, a.L.length), lv = level(a);
    row(d === 0 && a.summary.digest === b.summary.digest && lv.peak >= kSilentPeak, 'MONO',
      `polyMode ${mode} (${mode === 1 ? 'mono' : 'legato'}), three overlapping notes: stealFade 1 against 0 ${d === 0 ? 'bit-identical' : `differs by ${d.toExponential(2)}`} (digests ${a.summary.digest} and ${b.summary.digest}), peak ${lv.peak.toFixed(3)}`);
  }

  // ---- liveness
  for (const pool of kPools) {
    const a = level(R(`J/${pool}/1/whole/${kBlocks[0]}`)), b = level(R(`J/${pool}/0/whole/${kBlocks[0]}`)), db = 20 * Math.log10(a.rms / b.rms);
    row(a.peak >= kSilentPeak && b.peak >= kSilentPeak && Math.abs(db) <= kLiveDb, 'LIVENESS',
      `pool ${pool}, the six chords: stealFade 1 rms ${a.rms.toFixed(4)} (peak ${a.peak.toFixed(3)}) against stealFade 0 rms ${b.rms.toFixed(4)}: ${db.toFixed(3)} dB; required audible and within ${kLiveDb} dB`);
  }

  // ---- not judged: the pad with its noise blade, and with its own spread law
  for (const [id, what] of NOTES) {
    const line = fl => kNoteChords.map(k => `chord ${k} ${pct(jump(`N/${id}/${fl}`, k), 1)}`).join(', ');
    note(id.toUpperCase(), `${what}, pool 6 (NOT judged; it carries the shared-stream confound, see the header): stealFade 0: ${line(0)}; stealFade 1: ${line(1)}. ` +
      `Chord ${kFreeChord} takes no voice, so what it reads is the confound alone`);
  }

  console.log(`h2_steal_fade_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; the jump at ${kTakenChords.length} taken chords at pools ${kPools.join(' and ')} and at a pool of ${kWholePool} taken whole (tails and held voices, blade 2 off and on), its controls, the free-voice control, the fade's last tenth and its whole ramp, the start's lateness and step, the ${Object.keys(kPlants).length} planted faults, a held chord under a line and its control, block sizes ${kBlocks.join(', ')} with and without keepPhase, mono and legato, liveness)`);
  return red ? 1 : 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  try { process.exitCode = main(process.argv.slice(2)); }
  catch (e) {
    console.error(String(e && e.stack || e));
    console.log(`h2_steal_fade_check: RED (the run broke: ${String(e && e.message || e).split('\n')[0]})`);
    process.exitCode = 2;
  }
}
