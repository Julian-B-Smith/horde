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
 * WHAT IS RENDERED. Everything goes through the engine harness (h2/harness/), so this
 * is the C++ engine as it ships, never the JS one. One held note of
 * `ledger/breathing-pad`, 48 kHz, 8 s, host seed 1, at C3 (velocity 0.9), G3 (0.9) and
 * C4 (0.8), at os 2 and at os 1: tools/h2_frame_hold_check.mjs's own job and its own
 * detector (isolatedJumps, imported from it, not copied). THE PAD IS THE BANK'S AS
 * SHIPPED: unlike that check, this one writes NO patch setting over it, because B460's
 * acceptance reads the pad as shipped. Every job pins what is not the patch's: `os`,
 * `frameHold` 1, `stealFade` 0 and `spreadRamp`; `poly` is the harness cell's 8
 * (h2/harness/patches.json), and one note is played.
 *
 *   JUMP rows (judged; ROADMAP B460's added acceptance, to the letter: "the pad as
 *   shipped, with `frameHold` 1, reads 0 / 0 / 0 isolated jumps at C3, G3 and C4 at
 *   os 1 and os 2"). spreadRamp 1. Required: no isolated jump on any of the six renders.
 *
 *   CONTROL rows (judged; LIBRARY L0032). The same six renders with spreadRamp 0, the
 *   present law. Required: at least one isolated jump at each os, over the three
 *   notes, or the JUMP rows prove nothing and this check is RED whatever they read.
 *
 *   ATTRIB lines (NOTE, not judged). For every jump a JUMP row reads: its sample, its
 *   size and its place in the tick. A step of the spread law lands on the tick's
 *   second sample (position 1: the blade's output is one sample late); a jump
 *   anywhere else in the tick is not a tick's.
 *
 *   TWIN rows (judged). The pad's second blade is a NOISE blade (mode2 3, sample and
 *   hold): inside its window it steps from one random level to the next, and now and
 *   then two extreme draws make a step this detector reads as an isolated jump with no
 *   help from any law (measured 2026-10-13: 0.123 at C3, os 1, seed 1, at tick position
 *   11, with every spread value mid-ramp; and with the present law 0.147 at position
 *   16). The twin is the same job with that blade's depth 0 (`depth2` 0). A blade of
 *   depth 0 is silent and is still evaluated, so it draws from the engine's random
 *   stream exactly as before and the spread law walks the SAME path: what the twin
 *   leaves out is the noise blade's sound and nothing else. Required: the twin reads no
 *   isolated jump with spreadRamp 1 on any of the six renders; with spreadRamp 0 it
 *   reads at least one at each os (its control); and at each os it shares at least one
 *   jump, at the very same sample, with the pad as shipped (spreadRamp 0), or "the same
 *   path" is not shown and the row is red.
 *   The TWIN rows do NOT stand in for the JUMP rows: they are judged beside them.
 *
 *   TICK rows (judged against a ceiling that is PROPOSED, the human's to ratify). B460's
 *   own measure, as this check defines it (the row's text gives no formula): the second
 *   difference of the left pre-limiter bus, its mean square taken per position in the
 *   32-sample tick (the engine ticks on every 32nd sample from its first), and the
 *   largest of the first four positions over the median of all 32, in dB. One held C3
 *   of the pad as shipped (drift rate 0.1), with `driftRate` 1, and with the law off
 *   (`law` 0), at each os, spreadRamp 0 and 1. Measured 2026-10-13 (darwin-arm64):
 *   spreadRamp 0 reads 5.23 / 5.57 dB (os 2 / os 1) at drift rate 0.1 and 13.44 / 14.25
 *   at drift rate 1; spreadRamp 1 reads 0.36 / 0.04 and 0.50 / 0.22; the law off reads
 *   0.10 / 0.22 either way (a statistic that takes the largest of four positions reads
 *   a little over 0 on anything). Required: spreadRamp 1 at or under kTickCeil at both
 *   drift rates; spreadRamp 0 over it at both (its control). The law-off lines and the
 *   G3 and C4 lines are printed and not judged.
 *
 *   COMPOSE and PLANT rows (judged; h2/harness/control_spread.h says how each plant is
 *   built from shipped engines). The plants are read with the TICK measure, at os 1,
 *   where no decimation filter smooths a step.
 *     COMPOSE  control/ramp-both, the two-blade composition with nothing planted, must
 *              render what the engine renders on the same job, spreadRamp 1 and 0, to
 *              within kComposeCeil on the pre-limiter bus. The composition needs a job
 *              with no noise blade (control_spread.h), so these jobs are the pad with
 *              `mode2` 0, a plain blade in the noise blade's place.
 *     PLANT    control/ramp-blade1 (the ramp reaches blade 1 only) and
 *              control/ramp-blade2 (blade 2 only), on that job, and control/ramp-skip
 *              (the ramp dropped at one tick in four, so two ticks in four step) on the
 *              pad as shipped: each must read OVER kTickCeil, and the engine on the
 *              same job must read at or under it.
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
 * Deterministic: no clock, no randomness but the engine's seeded streams.
 */
import { realpathSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import { jobFor, patch, run } from '../h2/harness/harness.mjs';
import { isolatedJumps } from './h2_frame_hold_check.mjs';

const SR = 48000;
// The host random stream, the frame-hold check's. CHANGING THE SEED IS NEVER A FIX.
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
// the tick's positions a step of the law reaches: the step lands on position 1, and a
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
// Measured 2026-10-13: the composition with nothing planted is within 6.7e-16 of the
// engine on a bus that peaks at 0.70 (rounding in atanh and tanh). 1e-12 is about 1500
// times that, and six orders under the parity bound.
const kComposeCeil = 1e-12;
// liveness: flag on within this many dB of flag off, and neither silent (the
// frame-hold check's bound). Measured 2026-10-13: the largest |difference| over the
// twelve pairs was 0.89 dB (the pad at os 1, C3: the noise blade draws differently
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
const describe = j => (j.length ? `${j.length} (${j.map(e => `${e.step.toFixed(3)} at ${e.at}, tick position ${e.at % kTick}`).join('; ')})` : '0');
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

const padJob = (n, os, over, engine) => jobFor({ patch: PAD, engine, sr: SR, seed: SEED, seconds: kSeconds, notes: [n.note], vel: n.vel, pre: true,
  overrides: { os, ...kPinned, ...over } });

export function main(argv) {
  const bi = argv.indexOf('--bin'), opt = bi >= 0 ? { bin: argv[bi + 1] } : {};
  let red = 0;
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(8)} ${text}`); if (!ok) red++; };
  const note = (tag, text) => console.log(`NOTE  ${tag.padEnd(8)} ${text}`);
  patch(PAD);   // throws if the pad has left the reference set

  // One process renders everything.
  const jobs = [], at = {};
  const add = (key, job) => { at[key] = jobs.length; jobs.push(job); };
  for (const os of kOsList) {
    for (const n of NOTES) for (const sr of [1, 0]) {
      add(`pad/${os}/${n.name}/${sr}`, padJob(n, os, { spreadRamp: sr }));
      add(`twin/${os}/${n.name}/${sr}`, padJob(n, os, { ...kTwin, spreadRamp: sr }));
    }
    for (const [name, over] of kTickCases.slice(1)) for (const sr of [1, 0]) add(`tick/${os}/${name}/${sr}`, padJob(NOTES[0], os, { ...over, spreadRamp: sr }));
    // the plants (judged at kPlantOs, printed at the other)
    for (const sr of [1, 0]) {
      add(`plant/${os}/engine/${sr}`, padJob(kPlantNote, os, { ...kPlantJob, spreadRamp: sr }));
      add(`plant/${os}/both/${sr}`, padJob(kPlantNote, os, { ...kPlantJob, spreadRamp: sr }, 'control/ramp-both'));
    }
    add(`plant/${os}/blade1`, padJob(kPlantNote, os, { ...kPlantJob, spreadRamp: 1 }, 'control/ramp-blade1'));
    add(`plant/${os}/blade2`, padJob(kPlantNote, os, { ...kPlantJob, spreadRamp: 1 }, 'control/ramp-blade2'));
    add(`plant/${os}/skip`, padJob(kPlantNote, os, { spreadRamp: 1 }, 'control/ramp-skip'));
  }
  const rs = run(jobs, opt), R = key => rs[at[key]];
  const unknown = rs.filter(r => r.summary.unknownKeys);
  row(!unknown.length, 'KEYS', `the engine knew every key on ${rs.length - unknown.length} of ${rs.length} jobs${unknown.length ? `; first refused on: ${unknown[0].name}` : ''}`);
  console.log(`${PAD} as shipped, no patch setting written over it; pinned beside os and spreadRamp: ${JSON.stringify(kPinned)}; ${kSeconds} s at ${SR} Hz, seed ${SEED}, left pre-limiter bus`);
  const J = key => isolatedJumps(R(key).preL);

  // ---- the jumps: the pad as shipped (ROADMAP B460's acceptance), then its twin
  for (const os of kOsList) {
    let ctl = 0;
    const ctlText = [];
    for (const n of NOTES) {
      const on = J(`pad/${os}/${n.name}/1`), off = J(`pad/${os}/${n.name}/0`);
      row(on.length === 0, 'JUMP', `os ${os} ${n.name}, the pad as shipped, frameHold 1, spreadRamp 1: isolated jumps ${describe(on)}; required 0`);
      for (const e of on) {
        const tw = J(`twin/${os}/${n.name}/1`);
        note('ATTRIB', `os ${os} ${n.name} (not judged): the jump of ${e.step.toFixed(3)} at sample ${e.at} stands at tick position ${e.at % kTick}` +
          `${e.at % kTick === 1 ? ', where a step of the spread law lands' : ': a step of the spread law lands on position 1, so this one is not a tick\'s'}; ` +
          `the same render with the pad's noise blade silent (depth2 0, the same random stream) reads ${describe(tw)}`);
      }
      ctl += off.length; ctlText.push(`${n.name} ${describe(off)}`);
    }
    row(ctl > 0, 'CONTROL', `os ${os}, the pad as shipped, frameHold 1, spreadRamp 0 (the present law): isolated jumps ${ctlText.join(' / ')}; required at least 1 over the three notes`);
  }
  console.log(`the twin: the same jobs with ${JSON.stringify(kTwin)} (the noise blade silent and still drawing; header)`);
  for (const os of kOsList) {
    let ctl = 0, shared = 0;
    const ctlText = [];
    for (const n of NOTES) {
      const on = J(`twin/${os}/${n.name}/1`), off = J(`twin/${os}/${n.name}/0`), pad = J(`pad/${os}/${n.name}/0`);
      row(on.length === 0, 'TWIN', `os ${os} ${n.name}, noise blade silent, frameHold 1, spreadRamp 1: isolated jumps ${describe(on)}; required 0`);
      ctl += off.length; ctlText.push(`${n.name} ${describe(off)}`);
      shared += off.filter(a => pad.some(b => b.at === a.at)).length;
    }
    row(ctl > 0, 'TWIN', `os ${os}, noise blade silent, spreadRamp 0 (its control): isolated jumps ${ctlText.join(' / ')}; required at least 1 over the three notes`);
    row(shared > 0, 'TWIN', `os ${os}: the twin walks the pad's own spread: with spreadRamp 0 it shares ${shared} jump sample(s) with the pad as shipped; required at least 1`);
  }

  // ---- B460's own measure
  console.log(`tick energy: the second difference's mean square at the first ${kTickFirst} positions of the ${kTick}-sample tick (the largest) over the median position; ceiling ${dB(kTickCeil)}, PROPOSED (the human's to ratify)`);
  const T = key => tickDb(R(key).preL);
  for (const os of kOsList) {
    for (const [name, , judged] of kTickCases) {
      const key = sr => (name === kTickCases[0][0] ? `pad/${os}/${NOTES[0].name}/${sr}` : `tick/${os}/${name}/${sr}`);
      const on = T(key(1)), off = T(key(0));
      if (!judged) { note('TICK', `os ${os} C3, ${name} (not judged: the floor of the measure): spreadRamp 1 reads ${dB(on)}, spreadRamp 0 reads ${dB(off)}`); continue; }
      row(on <= kTickCeil, 'TICK', `os ${os} C3, ${name}, spreadRamp 1: ${dB(on)}; required at or under ${dB(kTickCeil)} (PROPOSED)`);
      row(off > kTickCeil, 'TICK', `os ${os} C3, ${name}, spreadRamp 0 (its control): ${dB(off)}; required over ${dB(kTickCeil)}`);
    }
    for (const n of NOTES.slice(1)) note('TICK', `os ${os} ${n.name}, ${kTickCases[0][0]} (not judged): spreadRamp 1 reads ${dB(T(`pad/${os}/${n.name}/1`))}, spreadRamp 0 reads ${dB(T(`pad/${os}/${n.name}/0`))}`);
  }

  // ---- the plants
  console.log(`plants (h2/harness/control_spread.h), one held ${kPlantNote.name}, read with the tick measure; judged at os ${kPlantOs}; the composition's job is the pad with ${JSON.stringify(kPlantJob)} (no noise blade)`);
  for (const os of kOsList) {
    const judged = os === kPlantOs;
    const P = k => R(`plant/${os}/${k}`);
    for (const sr of [1, 0]) {
      const d = maxDiffPre(P(`both/${sr}`), P(`engine/${sr}`));
      const text = `os ${os}, spreadRamp ${sr}: control/ramp-both (nothing planted) against the engine on the same job: max |difference| ${d.toExponential(2)} on the pre-limiter bus`;
      if (judged) row(d <= kComposeCeil, 'COMPOSE', `${text}; required at or under ${kComposeCeil}`); else note('COMPOSE', `${text} (not judged at this os)`);
    }
    const eng = tickDb(P('engine/1').preL), pad = T(`pad/${os}/${kPlantNote.name}/1`);
    const plants = [['control/ramp-blade1', 'the ramp reaches blade 1 only', 'blade1', eng, 'the engine on the same job'],
      ['control/ramp-blade2', 'the ramp reaches blade 2 only', 'blade2', eng, 'the engine on the same job'],
      ['control/ramp-skip', 'the ramp dropped at one tick in four', 'skip', pad, 'the engine on the same job, the pad as shipped']];
    if (judged) row(eng <= kTickCeil, 'PLANT', `os ${os}, the engine on the composition's job, spreadRamp 1: ${dB(eng)}; required at or under ${dB(kTickCeil)} (PROPOSED)`);
    for (const [id, what, key, ref, refName] of plants) {
      const v = tickDb(P(key).preL);
      const text = `os ${os}, ${id} (${what}): ${dB(v)}; ${refName} reads ${dB(ref)}`;
      if (judged) row(v > kTickCeil, 'PLANT', `${text}; the plant is required over ${dB(kTickCeil)}, or this check cannot see that fault`);
      else note('PLANT', `${text} (not judged at this os: the decimation filter spreads a step over several samples${v > kTickCeil ? '' : `, and this measure does NOT see this plant here: it reads under the ceiling of ${dB(kTickCeil)}`})`);
    }
  }

  // ---- liveness: every spreadRamp 1 render audible, and as loud as its spreadRamp 0 twin
  const live = [];
  for (const os of kOsList) for (const n of NOTES) for (const k of ['pad', 'twin']) live.push([`${k === 'pad' ? 'the pad' : 'the twin'} os ${os} ${n.name}`, `${k}/${os}/${n.name}`]);
  let liveBad = 0, liveWorst = 0, liveWorstName = '', firstBad = '';
  for (const [name, key] of live) {
    const a = level(R(key + '/1')), b = level(R(key + '/0')), db = 20 * Math.log10(a.rms / b.rms);
    if (!(a.peak >= kSilentPeak && b.peak >= kSilentPeak && Math.abs(db) <= kLiveDb)) { liveBad++; firstBad = firstBad || `${name}: rms ${a.rms.toExponential(2)} against ${b.rms.toExponential(2)} (${db.toFixed(2)} dB)`; }
    if (Math.abs(db) > liveWorst) { liveWorst = Math.abs(db); liveWorstName = name; }
  }
  row(liveBad === 0, 'LIVENESS', `${live.length - liveBad} of ${live.length} spreadRamp 1 renders are audible and within ${kLiveDb} dB of their spreadRamp 0 twin in rms (largest ${liveWorst.toFixed(2)} dB, ${liveWorstName})` +
    `${firstBad ? `; first failure: ${firstBad}` : ''}`);

  console.log(`h2_spread_ramp_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; the pad as shipped at os ${kOsList.join(' and ')}, its control and its twin, the tick measure, the composition and three plants, liveness)`);
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
