/*
 * h2_steal_fade_check.mjs — ADR-216 D7 (`stealFade`, ROADMAP B459) does what it was
 * ruled to do, on the C++ engine: with the flag on, a chord that takes sounding
 * voices makes no jump in the output, its notes start when the fade ends and from
 * silence, and nothing depends on the host's block size; with the flag off (the
 * present law) the same chords read the jump.
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
 *   BLOCKS rows (judged). The whole six-chord script at host blocks of 16, 37 and
 *   128 frames. With the flag on the three renders must agree at least as well as
 *   they do with it off: the largest |difference| from the 128-frame render, flag on,
 *   is at most the same figure with the flag off. The fade's end falls inside a host
 *   block at 37 and at 128, so this is the engine cutting its own render there.
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
 *   kJumpCeil   0.5 %. Worst 0.227 % (pool 6, chord 3); the margin is about twice
 *               that. What it bounds: over four samples a 240-sample ramp removes
 *               at most 3/240 of the taken voices' sound.
 *   kEndCeil    0.05 %. Worst 0.016 % (pool 8, chord 5); about three times that.
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
const kControlMin = 0.05, kFreeCeil = 0.01;   // ROADMAP B459's acceptance
// Liveness: measured 2026-10-13, the flag-on six-chord renders are 0.31 dB (pool 6)
// and 0.13 dB (pool 8) under their flag-off twins: a restarted voice attacks from its
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

export function main(argv) {
  const bi = argv.indexOf('--bin'), opt = bi >= 0 ? { bin: argv[bi + 1] } : {};
  let red = 0;
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(8)} ${text}`); if (!ok) red++; };
  const note = (tag, text) => console.log(`NOTE  ${tag.padEnd(8)} ${text}`);
  patch(PAD);   // throws if the pad has left the reference set

  // One process renders everything.
  const jobs = [], at = {};
  const add = (key, cmds, block) => { at[key] = jobs.length; jobs.push(jobFor({ patch: PAD, sr: SR, seed: SEED, block, cmds, name: key })); };
  for (const pool of kPools) for (const fl of [1, 0]) {
    for (const b of pool === 6 ? kBlocks : [kBlocks[0]]) add(`J/${pool}/${fl}/whole/${b}`, whole(kJudgedPinned, pool, fl), b);
    for (const k of [kFreeChord, ...kTakenChords]) add(`J/${pool}/${fl}/without/${k}`, without(kJudgedPinned, pool, fl, k));
  }
  for (const pool of kPools) for (const k of kAllWait[pool]) add(`J/${pool}/cancel/${k}`, cancelled(kJudgedPinned, pool, 1, k));
  for (const mode of [1, 2]) for (const fl of [1, 0]) add(`M/${mode}/${fl}`, monoPhrase(mode, fl));
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

  // ---- the host's block size
  const spread = fl => {
    const ref = R(`J/6/${fl}/whole/${kBlocks[0]}`);
    return kBlocks.slice(1).map(b => { const r = R(`J/6/${fl}/whole/${b}`); return { b, d: maxDiff(ref, 0, r, 0, ref.L.length), same: r.summary.digest === ref.summary.digest }; });
  };
  const bOn = spread(1), bOff = spread(0);
  const worstB = s => s.reduce((m, x) => (x.d <= m ? m : x.d), 0);
  const tell = s => s.map(x => `${x.b} frames ${x.d === 0 ? 'bit-identical' : x.d.toExponential(2)}${x.same === (x.d === 0) ? '' : ' (DIGEST DISAGREES)'}`).join(', ');
  row(worstB(bOn) <= worstB(bOff) && [...bOn, ...bOff].every(x => x.same === (x.d === 0)), 'BLOCKS',
    `the six chords at pool 6, largest |difference| from the ${kBlocks[0]}-frame render: stealFade 1: ${tell(bOn)}; stealFade 0: ${tell(bOff)}; required: the flag adds none`);

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

  console.log(`h2_steal_fade_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; the jump at ${kTakenChords.length} taken chords at pools ${kPools.join(' and ')}, its controls, the free-voice control, the start's lateness and step, block sizes ${kBlocks.join(', ')}, mono and legato, liveness)`);
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
