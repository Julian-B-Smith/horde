/*
 * h2_spread_ramp_check.mjs — ADR-216 D8 (`spreadRamp`, ROADMAP B460) does what it was
 * built to do, on the C++ engine: with the flag on, the member spread values reach the
 * blades as ramps across the couple tick, and a held note of the bank's Breathing pad
 * carries no step of the spread law's making; with it off (the present law) it does.
 *
 * WIRED: ./verify full
 *
 * THE FAULT (B460). The spread law recomputes every member's values (position, cut,
 * rotation, width, depth, index, the sine->saw band-limit) once per 32 samples, and the
 * blades read them every sample, so each tick's change arrives in one sample. Under
 * the drift law (law 4) that is a random step of every blade's position 1500 times a
 * second: a steady grain at the tick rate and, now and then, a jump the frame-hold
 * check's detector reads. D8 hands each value to its blade as a ramp across the tick
 * (h2/engine/engine.h, rampSpread).
 *
 * WHAT IS JUDGED, AND WHO RULED IT. ROADMAP B460's first added acceptance was "the pad
 * as shipped, with `frameHold` 1, reads 0 / 0 / 0 isolated jumps at C3, G3 and C4 at
 * os 1 and os 2". IT IS NOT MET AT OS 1, and no ramp can meet it: the pad's second
 * blade is a NOISE blade (mode2 3, sample and hold), which steps from one random level
 * to the next inside its window, and now and then two extreme draws make a step this
 * detector reads with no help from any law (ROADMAP B481; measured 2026-10-13: 0.123 at
 * C3, os 1, seed 1, at tick position 11, with every spread value mid-ramp). The lead
 * therefore ruled what is judged instead (PROPOSED, ADR-216 A4, the human's to ratify):
 * "on the pad as shipped, no isolated jump at a tick boundary; on its twin with the
 * noise blade silent, no isolated jump at all; each over eight seeds, three notes and
 * both os. The pad's total is printed with each remaining jump attributed, and is not
 * judged."
 *
 * WHAT IS RENDERED. Everything goes through the engine harness (h2/harness/), so this
 * is the C++ engine as it ships, never the JS one. One held note of
 * `ledger/breathing-pad`, 48 kHz, 8 s, at C3 (velocity 0.9), G3 (0.9) and C4 (0.8), at
 * os 2 and at os 1, at host seeds 1 to 8: tools/h2_frame_hold_check.mjs's own job and
 * its own detector (isolatedJumps, imported from it, not copied), on eight seeds where
 * that check has one. THE PAD IS THE BANK'S AS SHIPPED: unlike that check, this one
 * writes NO patch setting over it. Every job pins what is not the patch's: `os`,
 * `frameHold` 1, `stealFade` 0 and `spreadRamp`; `poly` is the harness cell's 8
 * (h2/harness/patches.json), and one note is played.
 *
 * AT A TICK BOUNDARY: THE POSITIONS A STEP OF THE LAW CAN REACH, from the engine's
 * code and not from where a jump was seen. The tick falls on every 32nd sample counted
 * from the engine's first (position 0), and the law's values change there before any
 * blade is evaluated. A blade's output leaves one internal step late, and a PolyBLEP's
 * leading half is added to the step going out now (engine.h stepBlade:
 * `o = m.prev + blepOut`).
 *   os 1: positions 0 and 1. The blade's new value arrives at position 1; a BLEP's
 *         leading half, computed from the new values, at position 0; after that the
 *         values hold still for the rest of the tick.
 *   os 2: positions 0 to 6. The same step enters the decimation filter (two biquads, a
 *         fourth-order low-pass at 0.45 sr; engine.h setOS) at the tick sample's first
 *         internal step (the BLEP's half) or its second (the value), and the filter
 *         spreads it. Of a unit step the output moves, per position from 0:
 *         0.364, 0.792, -0.196, 0.046, -0.002, -0.008, 0.007, -0.004 (entering at the
 *         first internal step) and 0.068, 0.781, 0.259, -0.164, 0.084, -0.038, 0.015,
 *         -0.005 (at the second); computed from the filter's own coefficients. The set
 *         is every position at which either moves by at least 1 % of its largest move.
 * The detector names an event by its largest step, so a law's step is named at
 * position 1 nearly always (every one of the flag-off jumps measured); the wider sets
 * are judged so that one named a sample early or late is not let through. A jump of
 * the noise blade's falls in the set by chance about 2 times in 32 at os 1 and 7 in 32
 * at os 2, and would turn the row red; it is not excused here.
 *
 *   JUMP rows (judged, i). The pad as shipped, spreadRamp 1: no isolated jump at a tick
 *   boundary, over the 24 renders of each os (eight seeds, three notes).
 *
 *   CONTROL rows (judged; LIBRARY L0032). The same renders with spreadRamp 0, the
 *   present law. Required: at least one isolated jump at a tick boundary at each os,
 *   or the JUMP rows prove nothing and this check is RED whatever they read.
 *   (Measured 2026-10-13: 4 of its 4 jumps at os 2 and 14 of its 15 at os 1.)
 *
 *   TOTAL and ATTRIB lines (NOTE, not judged). The pad as shipped's whole count of
 *   isolated jumps with spreadRamp 1, per os, and each one listed: its size, sample,
 *   seed, note and place in the tick, and what the twin of that very render reads.
 *
 *   TWIN rows (judged, ii). The twin is the same job with the noise blade's depth 0
 *   (`depth2` 0). A blade of depth 0 is silent and is still evaluated, so it draws from
 *   the engine's random stream exactly as before and the spread law walks the SAME
 *   path: what the twin leaves out is the noise blade's sound and nothing else.
 *   Required: with spreadRamp 1 the twin reads no isolated jump AT ALL on the 24
 *   renders of each os; with spreadRamp 0 it reads at least one at each os (its
 *   control); and at each os it shares at least one jump, at the very same sample,
 *   with the pad as shipped (spreadRamp 0), or "the same path" is not shown.
 *
 *   SKIP row (judged): the tick-boundary reading is not blind to the fault it is for.
 *   The 24 os 1 jobs of row (i) are also played through control/ramp-skip
 *   (h2/harness/control_spread.h: the ramp dropped at one tick in four, so two ticks
 *   in four step) and read exactly as row (i) reads the engine. Required: at least one
 *   isolated jump at a tick boundary, which is to say that row (i) would be red on an
 *   engine that left those steps (measured 2026-10-13: 9). Judged at os 1 only, to keep the run short: at os 2
 *   the present law itself reads only 4 on its 24 renders, and half as many steps
 *   would be a thin control.
 *
 *   TICK rows (judged against a ceiling that is PROPOSED, the human's to ratify). B460's
 *   own measure, as this check defines it (the row's text gives no formula): the second
 *   difference of the left pre-limiter bus, its mean square taken per position in the
 *   32-sample tick, and the largest of the first four positions over the median of all
 *   32, in dB. One held C3 at seed 1 of the pad as shipped (drift rate 0.1), with
 *   `driftRate` 1, and with the law off (`law` 0), at each os, spreadRamp 0 and 1.
 *   Measured 2026-10-13 (darwin-arm64): spreadRamp 0 reads 5.23 / 5.57 dB (os 2 / os 1)
 *   at drift rate 0.1 and 13.44 / 14.25 at drift rate 1; spreadRamp 1 reads 0.36 / 0.04
 *   and 0.50 / 0.22; the law off reads 0.10 / 0.22 either way (a statistic that takes
 *   the largest of four positions reads a little over 0 on anything). Required:
 *   spreadRamp 1 at or under kTickCeil at both drift rates; spreadRamp 0 over it at
 *   both (its control). The law-off lines and the G3 and C4 lines are printed and not
 *   judged.
 *
 *   COMPOSE and PLANT rows (judged; h2/harness/control_spread.h says how each plant is
 *   built from shipped engines). The plants are read with the TICK measure, at os 1,
 *   where no decimation filter smooths a step, at seed 1.
 *     COMPOSE  control/ramp-both, the two-blade composition with nothing planted, must
 *              render what the engine renders on the same job, spreadRamp 1 and 0, to
 *              within kComposeCeil on the pre-limiter bus. The composition needs a job
 *              with no noise blade (control_spread.h), so these jobs are the pad with
 *              `mode2` 0, a plain blade in the noise blade's place.
 *     PLANT    control/ramp-blade1 (the ramp reaches blade 1 only) and
 *              control/ramp-blade2 (blade 2 only), on that job, and control/ramp-skip
 *              on the pad as shipped: each must read OVER kTickCeil, and the engine on
 *              the same job must read at or under it.
 *     At os 2 the same plants are printed, not judged: there the decimation filter
 *     spreads a step over several samples and this measure reads control/ramp-blade1
 *     at 0.60 dB, UNDER the ceiling. That blind spot is stated on every run.
 *
 *   LIVENESS rows (judged). A silent render reads no jump and no tick. Every spreadRamp 1
 *   render must be audible and within kLiveDb of its spreadRamp 0 twin in rms.
 *
 * ALSO REQUIRED OF EVERY JOB: the engine knew every key (a misspelt `spreadRamp` would
 * render the present law twice).
 *
 * THE LAST LINE IS THE VERDICT: `h2_spread_ramp_check: GREEN (...)` or `RED (...)`.
 * Exit 0 green, 1 red, 2 the run broke (no binary, a refused job).
 *
 *   node tools/h2_spread_ramp_check.mjs [--bin <h2_harness_render>]
 *
 * Deterministic: no clock, no randomness but the engine's seeded streams. 77 s here
 * (2026-10-13, darwin-arm64): 237 jobs, one render process per seed, so that no more
 * than one seed's samples are held at a time.
 */
import { realpathSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import { jobFor, patch, run } from '../h2/harness/harness.mjs';
import { isolatedJumps } from './h2_frame_hold_check.mjs';

const SR = 48000;
// The host random streams (ADR-216 A4, PROPOSED: eight seeds). The tick measure and the
// plants read seed 1, the frame-hold check's. CHANGING A SEED IS NEVER A FIX.
const kSeeds = [1, 2, 3, 4, 5, 6, 7, 8];
const SEED = 1;
const PAD = 'ledger/breathing-pad';
const NOTES = [{ name: 'C3', note: 48, vel: 0.9 }, { name: 'G3', note: 55, vel: 0.9 }, { name: 'C4', note: 60, vel: 0.8 }];
const kSeconds = 8;
const kOsList = [2, 1];
// what every job pins beside `os` and `spreadRamp` (header)
const kPinned = { frameHold: 1, stealFade: 0 };
// the twin: the pad's noise blade silent, the random stream untouched (header)
const kTwin = { depth2: 0 };
// the engine's couple tick, in samples (h2/engine/engine.h kCoupleSamples)
const kTick = 32;
// the last tick position a step of the law can reach, per os (header, AT A TICK BOUNDARY)
const kBoundaryLast = { 1: 1, 2: 6 };
const atBoundary = (os, at) => at % kTick <= kBoundaryLast[os];
// the tick's positions the tick measure reads: the step lands on position 1, and a
// second difference there is read from positions 0 to 2; the fourth is margin
const kTickFirst = 4;
// PROPOSED, the human's to ratify: the measured spreadRamp 1 worst (0.50 dB: C3, os 2,
// drift rate 1; 2026-10-13, darwin-arm64) times two. The weakest spreadRamp 0 reading
// judged against it is 5.23 dB, and the weakest plant 2.51 dB.
const kTickCeil = 1.0;
const kTickCases = [['drift rate 0.1 (as shipped)', {}, true], ['drift rate 1', { driftRate: 1 }, true], ['the law off (law 0)', { law: 0 }, false]];
// the composition's job: no noise blade (header, COMPOSE)
const kPlantJob = { mode2: 0 };
const kPlantOs = 1, kPlantNote = NOTES[0];
// PROPOSED. Measured 2026-10-13: the composition with nothing planted is within 6.7e-16
// of the engine on a bus that peaks at 0.70 (rounding in atanh and tanh). 1e-12 is about
// 1500 times that, and six orders under the parity bound.
const kComposeCeil = 1e-12;
// liveness: flag on within this many dB of flag off, and neither silent (the
// frame-hold check's bound). Measured 2026-10-13: the largest |difference| over the 96
// pairs was 0.91 dB (the pad at seed 7, os 2, C3: the noise blade draws differently
// once its position moves differently).
const kLiveDb = 3;
const kSilentPeak = Math.pow(10, -90 / 20);   // tools/patchspace/metrics.mjs's silence line, -90 dBFS peak

/* B460's measure (header, TICK): dB, the first positions of the tick over the median position */
export function tickDb(x) {
  const E = new Float64Array(kTick), C = new Float64Array(kTick);
  for (let n = 1; n < x.length - 1; n++) { const d = x[n + 1] - 2 * x[n] + x[n - 1]; E[n % kTick] += d * d; C[n % kTick]++; }
  for (let p = 0; p < kTick; p++) E[p] /= C[p];
  const s = Float64Array.from(E).sort(), med = 0.5 * (s[kTick / 2 - 1] + s[kTick / 2]);
  let first = 0;
  for (let p = 0; p < kTickFirst; p++) if (E[p] > first) first = E[p];
  return 10 * Math.log10(first / med);
}
const dB = x => `${x.toFixed(2)} dB`;
/* largest |a - b| over the pre-limiter bus, both channels */
function maxDiffPre(a, b) {
  let m = 0;
  for (let i = 0; i < a.preL.length; i++) {
    const l = Math.abs(a.preL[i] - b.preL[i]), r = Math.abs(a.preR[i] - b.preR[i]);
    if (!(l <= m)) m = l;   // a NaN difference is kept, and fails every bound
    if (!(r <= m)) m = r;
  }
  return m;
}
function level(r) {
  let e = 0, pk = 0;
  for (let i = 0; i < r.L.length; i++) { e += r.L[i] * r.L[i] + r.R[i] * r.R[i]; pk = Math.max(pk, Math.abs(r.L[i]), Math.abs(r.R[i])); }
  return { rms: Math.sqrt(e / (2 * r.L.length)), peak: pk };
}

const padJob = (seed, n, os, over, engine) => jobFor({ patch: PAD, engine, sr: SR, seed, seconds: kSeconds, notes: [n.note], vel: n.vel, pre: true,
  overrides: { os, ...kPinned, ...over } });

export function main(argv) {
  const bi = argv.indexOf('--bin'), opt = bi >= 0 ? { bin: argv[bi + 1] } : {};
  let red = 0;
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(8)} ${text}`); if (!ok) red++; };
  const note = (tag, text) => console.log(`NOTE  ${tag.padEnd(8)} ${text}`);
  patch(PAD);   // throws if the pad has left the reference set

  // One render process per seed; only the readings are kept.
  const cells = {};          // `${kind}/${os}/${sr}` -> [{ seed, note, jumps }], kind pad | twin | skip
  const live = [];           // [name, flag-on level, flag-off level]
  const tick = {}, diff = {};   // seed 1: the tick measure per job, the composition against the engine
  let jobsN = 0, unknownN = 0, firstUnknown = '';
  for (const seed of kSeeds) {
    const jobs = [], at = {};
    const add = (key, job) => { at[key] = jobs.length; jobs.push(job); };
    for (const os of kOsList) for (const n of NOTES) for (const sr of [1, 0]) {
      add(`pad/${os}/${n.name}/${sr}`, padJob(seed, n, os, { spreadRamp: sr }));
      add(`twin/${os}/${n.name}/${sr}`, padJob(seed, n, os, { ...kTwin, spreadRamp: sr }));
    }
    // row (i)'s own jobs through the skipped-tick plant (header, SKIP)
    for (const n of NOTES) add(`skip/${kPlantOs}/${n.name}/1`, padJob(seed, n, kPlantOs, { spreadRamp: 1 }, 'control/ramp-skip'));
    if (seed === SEED) for (const os of kOsList) {
      for (const [name, over] of kTickCases.slice(1)) for (const sr of [1, 0]) add(`tick/${os}/${name}/${sr}`, padJob(seed, NOTES[0], os, { ...over, spreadRamp: sr }));
      for (const sr of [1, 0]) {
        add(`plant/${os}/engine/${sr}`, padJob(seed, kPlantNote, os, { ...kPlantJob, spreadRamp: sr }));
        add(`plant/${os}/both/${sr}`, padJob(seed, kPlantNote, os, { ...kPlantJob, spreadRamp: sr }, 'control/ramp-both'));
      }
      add(`plant/${os}/blade1`, padJob(seed, kPlantNote, os, { ...kPlantJob, spreadRamp: 1 }, 'control/ramp-blade1'));
      add(`plant/${os}/blade2`, padJob(seed, kPlantNote, os, { ...kPlantJob, spreadRamp: 1 }, 'control/ramp-blade2'));
      if (os !== kPlantOs) add(`skip/${os}/${kPlantNote.name}/1`, padJob(seed, kPlantNote, os, { spreadRamp: 1 }, 'control/ramp-skip'));
    }
    const rs = run(jobs, opt), R = key => rs[at[key]];
    jobsN += rs.length;
    for (const r of rs) if (r.summary.unknownKeys) { unknownN++; firstUnknown = firstUnknown || r.name; }
    for (const key of Object.keys(at)) {
      const [kind, os, name, sr] = key.split('/');
      if (kind === 'pad' || kind === 'twin' || (kind === 'skip' && +os === kPlantOs))
        (cells[`${kind}/${os}/${sr}`] ||= []).push({ seed, note: name, jumps: isolatedJumps(R(key).preL) });
    }
    for (const os of kOsList) for (const n of NOTES) for (const k of ['pad', 'twin'])
      live.push([`${k === 'pad' ? 'the pad' : 'the twin'} seed ${seed} os ${os} ${n.name}`, level(R(`${k}/${os}/${n.name}/1`)), level(R(`${k}/${os}/${n.name}/0`))]);
    if (seed === SEED) {
      for (const key of Object.keys(at)) tick[key] = tickDb(R(key).preL);
      for (const os of kOsList) for (const sr of [1, 0]) diff[`${os}/${sr}`] = maxDiffPre(R(`plant/${os}/both/${sr}`), R(`plant/${os}/engine/${sr}`));
    }
  }
  row(!unknownN, 'KEYS', `the engine knew every key on ${jobsN - unknownN} of ${jobsN} jobs${unknownN ? `; first refused on: ${firstUnknown}` : ''}`);
  console.log(`${PAD} as shipped, no patch setting written over it; pinned beside os and spreadRamp: ${JSON.stringify(kPinned)}; ${kSeconds} s at ${SR} Hz, seeds ${kSeeds[0]} to ${kSeeds[kSeeds.length - 1]}, left pre-limiter bus`);
  console.log(`at a tick boundary: tick positions 0 to ${kBoundaryLast[1]} at os 1 and 0 to ${kBoundaryLast[2]} at os 2 (what a step of the law can reach; header)`);

  // every jump of a cell, flat, and how it prints
  const all = key => (cells[key] || []).flatMap(c => c.jumps.map(e => ({ ...e, seed: c.seed, note: c.note })));
  const one = e => `${e.step.toFixed(3)} at ${e.at} (seed ${e.seed}, ${e.note}, tick position ${e.at % kTick})`;
  const list = js => (js.length ? `${js.length}: ${js.map(one).join('; ')}` : '0');
  const renders = key => (cells[key] || []).length;

  // ---- (i) the pad as shipped: no isolated jump at a tick boundary
  for (const os of kOsList) {
    const on = all(`pad/${os}/1`), off = all(`pad/${os}/0`);
    const onB = on.filter(e => atBoundary(os, e.at)), offB = off.filter(e => atBoundary(os, e.at));
    row(onB.length === 0 && renders(`pad/${os}/1`) === kSeeds.length * NOTES.length, 'JUMP',
      `os ${os}, the pad as shipped, frameHold 1, spreadRamp 1, ${renders(`pad/${os}/1`)} renders: isolated jumps at a tick boundary ${list(onB)}; required 0`);
    row(offB.length > 0, 'CONTROL', `os ${os}, the same renders with spreadRamp 0 (the present law): isolated jumps at a tick boundary ${offB.length}, of ${off.length} in all; required at least 1`);
    note('TOTAL', `os ${os}, the pad as shipped, spreadRamp 1 (not judged: PROPOSED, ADR-216 A4): ${on.length} isolated jump(s) in all on ${renders(`pad/${os}/1`)} renders. ` +
      `ROADMAP B460's first added acceptance asked for 0 here; it is ${on.length ? 'NOT met' : 'met'} at this os`);
    for (const e of on) {
      const tw = (cells[`twin/${os}/1`] || []).find(c => c.seed === e.seed && c.note === e.note);
      note('ATTRIB', `os ${os} (not judged): ${one(e)} is ${atBoundary(os, e.at) ? 'AT a tick boundary' : 'not at a tick boundary, so not a step of the law'}; ` +
        `the twin of that render (noise blade silent, the same random stream) reads ${tw ? list(tw.jumps.map(j => ({ ...j, seed: e.seed, note: e.note }))) : 'nothing: no twin was rendered'}`);
    }
  }

  // ---- (ii) the twin: no isolated jump at all
  console.log(`the twin: the same jobs with ${JSON.stringify(kTwin)} (the noise blade silent and still drawing; header)`);
  for (const os of kOsList) {
    const on = all(`twin/${os}/1`), off = all(`twin/${os}/0`), pad = all(`pad/${os}/0`);
    row(on.length === 0 && renders(`twin/${os}/1`) === kSeeds.length * NOTES.length, 'TWIN',
      `os ${os}, noise blade silent, frameHold 1, spreadRamp 1, ${renders(`twin/${os}/1`)} renders: isolated jumps ${list(on)}; required 0`);
    row(off.length > 0, 'TWIN', `os ${os}, noise blade silent, spreadRamp 0 (its control): isolated jumps ${off.length}, ${off.filter(e => atBoundary(os, e.at)).length} of them at a tick boundary; required at least 1`);
    const shared = off.filter(a => pad.some(b => b.seed === a.seed && b.note === a.note && b.at === a.at)).length;
    row(shared > 0, 'TWIN', `os ${os}: the twin walks the pad's own spread: with spreadRamp 0 it shares ${shared} jump sample(s) with the pad as shipped (same seed, note and sample); required at least 1`);
  }

  // ---- the tick-boundary reading sees a step the ramp leaves
  {
    const os = kPlantOs, js = all(`skip/${os}/1`), b = js.filter(e => atBoundary(os, e.at));
    row(b.length > 0, 'SKIP', `os ${os}, control/ramp-skip (the ramp dropped at one tick in four) on row (i)'s ${renders(`skip/${os}/1`)} jobs, read as row (i) reads the engine: ` +
      `isolated jumps at a tick boundary ${b.length}, of ${js.length} in all; required at least 1, or row (i) could not see a step the ramp leaves`);
  }

  // ---- B460's own measure (seed 1)
  console.log(`tick energy (seed ${SEED}): the second difference's mean square at the first ${kTickFirst} positions of the ${kTick}-sample tick (the largest) over the median position; ceiling ${dB(kTickCeil)}, PROPOSED (the human's to ratify)`);
  for (const os of kOsList) {
    for (const [name, , judged] of kTickCases) {
      const key = sr => (name === kTickCases[0][0] ? `pad/${os}/${NOTES[0].name}/${sr}` : `tick/${os}/${name}/${sr}`);
      const on = tick[key(1)], off = tick[key(0)];
      if (!judged) { note('TICK', `os ${os} C3, ${name} (not judged: the floor of the measure): spreadRamp 1 reads ${dB(on)}, spreadRamp 0 reads ${dB(off)}`); continue; }
      row(on <= kTickCeil, 'TICK', `os ${os} C3, ${name}, spreadRamp 1: ${dB(on)}; required at or under ${dB(kTickCeil)} (PROPOSED)`);
      row(off > kTickCeil, 'TICK', `os ${os} C3, ${name}, spreadRamp 0 (its control): ${dB(off)}; required over ${dB(kTickCeil)}`);
    }
    for (const n of NOTES.slice(1)) note('TICK', `os ${os} ${n.name}, ${kTickCases[0][0]} (not judged): spreadRamp 1 reads ${dB(tick[`pad/${os}/${n.name}/1`])}, spreadRamp 0 reads ${dB(tick[`pad/${os}/${n.name}/0`])}`);
  }

  // ---- the plants (seed 1)
  console.log(`plants (h2/harness/control_spread.h), one held ${kPlantNote.name} at seed ${SEED}, read with the tick measure; judged at os ${kPlantOs}; the composition's job is the pad with ${JSON.stringify(kPlantJob)} (no noise blade)`);
  for (const os of kOsList) {
    const judged = os === kPlantOs;
    for (const sr of [1, 0]) {
      const d = diff[`${os}/${sr}`];
      const text = `os ${os}, spreadRamp ${sr}: control/ramp-both (nothing planted) against the engine on the same job: max |difference| ${d.toExponential(2)} on the pre-limiter bus`;
      if (judged) row(d <= kComposeCeil, 'COMPOSE', `${text}; required at or under ${kComposeCeil} (PROPOSED)`); else note('COMPOSE', `${text} (not judged at this os)`);
    }
    const eng = tick[`plant/${os}/engine/1`], pad = tick[`pad/${os}/${kPlantNote.name}/1`];
    const plants = [['control/ramp-blade1', 'the ramp reaches blade 1 only', `plant/${os}/blade1`, eng, 'the engine on the same job'],
      ['control/ramp-blade2', 'the ramp reaches blade 2 only', `plant/${os}/blade2`, eng, 'the engine on the same job'],
      ['control/ramp-skip', 'the ramp dropped at one tick in four', `skip/${os}/${kPlantNote.name}/1`, pad, 'the engine on the same job, the pad as shipped']];
    if (judged) row(eng <= kTickCeil, 'PLANT', `os ${os}, the engine on the composition's job, spreadRamp 1: ${dB(eng)}; required at or under ${dB(kTickCeil)} (PROPOSED)`);
    for (const [id, what, key, ref, refName] of plants) {
      const v = tick[key];
      const text = `os ${os}, ${id} (${what}): ${dB(v)}; ${refName} reads ${dB(ref)}`;
      if (judged) row(v > kTickCeil, 'PLANT', `${text}; the plant is required over ${dB(kTickCeil)}, or this check cannot see that fault`);
      else note('PLANT', `${text} (not judged at this os: the decimation filter spreads a step over several samples${v > kTickCeil ? '' : `, and this measure does NOT see this plant here: it reads under the ceiling of ${dB(kTickCeil)}`})`);
    }
  }

  // ---- liveness: every spreadRamp 1 render audible, and as loud as its spreadRamp 0 twin
  let liveBad = 0, liveWorst = 0, liveWorstName = '', firstBad = '';
  for (const [name, a, b] of live) {
    const db = 20 * Math.log10(a.rms / b.rms);
    if (!(a.peak >= kSilentPeak && b.peak >= kSilentPeak && Math.abs(db) <= kLiveDb)) { liveBad++; firstBad = firstBad || `${name}: rms ${a.rms.toExponential(2)} against ${b.rms.toExponential(2)} (${db.toFixed(2)} dB)`; }
    if (Math.abs(db) > liveWorst) { liveWorst = Math.abs(db); liveWorstName = name; }
  }
  row(liveBad === 0 && live.length > 0, 'LIVENESS', `${live.length - liveBad} of ${live.length} spreadRamp 1 renders are audible and within ${kLiveDb} dB of their spreadRamp 0 twin in rms (largest ${liveWorst.toFixed(2)} dB, ${liveWorstName})` +
    `${firstBad ? `; first failure: ${firstBad}` : ''}`);

  console.log(`h2_spread_ramp_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; the pad as shipped at a tick boundary and its twin with the noise blade silent, over ${kSeeds.length} seeds, ${NOTES.length} notes and os ${kOsList.join(' and ')}; ` +
    `their controls, the tick measure, the composition and three plants, liveness)`);
  return red ? 1 : 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  try { process.exitCode = main(process.argv.slice(2)); }
  catch (e) {
    console.error(String(e && e.stack || e));
    console.log(`h2_spread_ramp_check: RED (the run broke: ${String(e && e.message || e).split('\n')[0]})`);
    process.exitCode = 2;
  }
}
