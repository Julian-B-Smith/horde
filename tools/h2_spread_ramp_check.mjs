/*
 * h2_spread_ramp_check.mjs — ADR-216 D8 (`spreadRamp`, ROADMAP B460) does what it was
 * built to do, on the C++ engine: with the flag on, every kind of member spread value
 * reaches the blades as a ramp across the couple tick, and the spread law leaves no
 * step in a held note; with it off (the present law) it does.
 *
 * WIRED: ./verify full
 *
 * THE FAULT (B460). The spread law recomputes every member's values (position, cut,
 * rotation rate, width, depth, index, the sine->saw band-limit pair, for each blade)
 * once per 32 samples, and the blades read them every sample, so each tick's change
 * arrives in one sample. Under the drift law (law 4) that is a random step of every
 * blade's values 1500 times a second: a steady grain at the tick rate and, now and
 * then, a jump the frame-hold check's detector reads. D8 hands each value to its blade
 * as a ramp across the tick (h2/engine/engine.h, rampSpread).
 *
 * WHAT THIS CHECK ANSWERS, AND WHAT IT DOES NOT (PROPOSED, ADR-216 A4 items 6 to 9, the
 * human's to ratify). It answers whether the spread law's steps are gone. It does NOT
 * answer whether the bank's pad as shipped clicks: that pad's second blade is a NOISE
 * blade (mode2 3, sample and hold), which steps by itself (ROADMAP B481), and ROADMAP
 * B460's first added acceptance ("the pad as shipped, with `frameHold` 1, reads
 * 0 / 0 / 0 isolated jumps at C3, G3 and C4 at os 1 and os 2") is NOT met at os 1
 * for that reason. The pad as shipped and its noise-silent twin are PRINTED here with
 * every jump attributed, and not judged. Two earlier forms of this check judged them
 * and held only by the seeds chosen (an independent critic, 2026-10-14: at seed 14 a
 * noise jump of 0.141 falls on tick position 0; at C2 the twin's own wave edges read
 * as jumps); this one judges neither.
 *
 * WHAT IS RENDERED. Everything goes through the engine harness (h2/harness/), so this
 * is the C++ engine as it ships, never the JS one. 48 kHz; every job pins `os`,
 * `frameHold` 1, `stealFade` 0 and `spreadRamp`; `poly` is the harness cell's 8
 * (h2/harness/patches.json); one note is held for the whole render. The patch is
 * `ledger/breathing-pad` with the settings each section names written over it.
 *
 * THE WALK JOB (ADR-216 A4 item 7): the pad with its second blade in a plain mode at
 * depth 0 (`mode2` 0, `depth2` 0). With a noise blade the flag changes the spread's
 * random walk itself: the blade draws from the engine's one random stream at every
 * noise step, how many it takes depends on how its position moves, and the drift law
 * draws from the same stream; so on the pad as shipped a flag-on render and a
 * flag-off render are two different walks (true at depth 0 too). On the walk job
 * nothing draws but the law, and the two walk alike.
 *
 *   WALK rows (judged). Shown, not assumed: control/walk-probe
 *   (h2/harness/control_spread.h) plays one job through two engines, one with the
 *   ramp and one without, and after every render call reads both through the
 *   engine's public voice view: the largest difference between them over every
 *   member's fourteen spread slots and its phase. Required: exactly 0 on every walk
 *   job probed (all 40 note-and-seed pairs at os 1, and seed 1's five notes at os 2:
 *   the walk does not depend on os, and the five show it), with members compared; and
 *   on the pad AS SHIPPED the same probe must read MORE than 0 (its control: it can
 *   see two walks part).
 *
 *   JUMP rows (judged). The walk job, spreadRamp 1, at C2, G2, C3, G3 (velocity 0.9)
 *   and C4 (0.8), seeds 1 to 8, os 1 and os 2 (80 renders): no RAW DETECTOR HIT at a
 *   tick boundary. A raw hit is one sample at which tools/h2_frame_hold_check.mjs's
 *   detector fires (a step on the left pre-limiter bus over 0.02 and over 3 times the
 *   99th percentile of the steps around it), BEFORE that check merges hits within 480
 *   samples into one event named by its largest step: merged, a law's step can hide
 *   behind a larger step nearby that is not at a boundary. The detector is copied
 *   here to get at the raw hits (that check exports only the merged form), and the
 *   DETECTOR row holds the copy to the original: merged by the original's rule, the
 *   copy's hits must be the original's events, on every one of the 160 renders.
 *
 *   CONTROL rows (judged; LIBRARY L0032). The same renders with spreadRamp 0, the
 *   present law. Required: at least one raw hit at a tick boundary at each os.
 *
 *   SKIP row (judged). The os 1 walk jobs through control/ramp-skip (the ramp dropped
 *   at one tick in four, so two ticks in four step), read as the JUMP row reads the
 *   engine. Required: at least one raw hit at a tick boundary, which is to say the
 *   JUMP row would be red on an engine that left those steps.
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
 * A hit that is not the law's (the wave's own edge) falls in the set by chance about 2
 * times in 32 at os 1 and 7 in 32 at os 2, and would turn the JUMP row red; it is not
 * excused here.
 *
 * EVERY KIND OF VALUE (ADR-216 A4 item 6). The pad moves only position (its other
 * spreads are 0 and its cut is quantized), so a ramp that reached position alone
 * would pass every row above. Two sets of rows close that, on jobs that move each
 * kind (blade 2 owning its spreads, `b2sp` 1; no noise blade; cut not quantized).
 *
 *   PROBE rows (judged): THE VALUES THEMSELVES. control/ramp-probe plays a job that
 *   moves every value on both blades through two engines, one with the ramp and one
 *   without, one sample at a time, and reads each of the eighteen values of every
 *   member after every sample through the engine's public voice view. Over each tick
 *   a value that moved is scored by how far its moves per sample are from equal
 *   shares (control_spread.h, probe(): 0 for a ramp, 15 for a step at the tick).
 *   Required, per kind and per blade: the engine with the ramp scores at or under
 *   kProbeCeil on every value of the kind, over at least one tick on which it moved;
 *   and THE PLANT, the same kind's values taken from the engine without the ramp
 *   ("that kind left stepped, everything else ramped"), scores at least kProbeStep.
 *   Two jobs, because a cut is an offset without a cut rule and a ratio with one. A
 *   value that moved on neither job is printed UNPROVEN and counted in the verdict
 *   line; it does not turn the check red. This row also sees a ramp that ends one
 *   sample short, which no measure of the output here can.
 *
 *   KIND rows (the SOUND; judged for the kinds the tick measure can hear). Per kind
 *   and per blade, a job in which ONE blade moves that kind and nothing else while
 *   the other moves every kind, at the fastest drift (`driftRate` 12), os 1, C3,
 *   seed 1, 4 s; and control/step-<kind>-b<n>, which renders that job with the one
 *   blade's values stepped and the other's ramped (three shipped engines summed on
 *   the pre-limiter bus; control_spread.h). Read with the tick measure (below).
 *   Required for position, cut, width and index: the engine at or under kTickCeil,
 *   the plant over it, and control/ramp-both (the same composition, nothing planted)
 *   within kComposeCeil of the engine. For ROTATION RATE, DEPTH and THE BAND-LIMIT
 *   PAIR the tick measure does not tell the plant from the engine (a rate's step is a
 *   kink in a position, not a step; a depth's is a few per cent of one blade's level;
 *   the pair stepped together stays a matched pair): their lines are printed, NOT
 *   judged, and those kinds rest on the PROBE rows alone. ONE LINE THERE IS OVER THE
 *   CEILING AND IS NOT JUDGED: on the band-limit job the engine itself, ramp on, reads
 *   about 2.5 dB at this drift rate (24 dB with the ramp off). It is printed on every
 *   run, marked, and is the lead's to rule (the trace, round 3).
 *
 * THE TICK MEASURE (TICK rows, judged against a ceiling that is PROPOSED). B460's own
 * measure, as this check defines it (the row's text gives no formula): the second
 * difference of the left pre-limiter bus, its mean square taken per position in the
 * 32-sample tick, and the largest of the first four positions over the median of all
 * 32, in dB. One held C3 at seed 1 of the pad as shipped (drift rate 0.1), with
 * `driftRate` 1, and with the law off (`law` 0), at each os, spreadRamp 0 and 1.
 * Measured 2026-10-13 (darwin-arm64): spreadRamp 0 reads 5.23 / 5.57 dB (os 2 / os 1)
 * at drift rate 0.1 and 13.44 / 14.25 at drift rate 1; spreadRamp 1 reads 0.36 / 0.04
 * and 0.50 / 0.22; the law off reads 0.10 / 0.22 either way (a statistic that takes
 * the largest of four positions reads a little over 0 on anything). Required:
 * spreadRamp 1 at or under kTickCeil at both drift rates; spreadRamp 0 over it at both.
 *
 * COMPOSE and PLANT rows (judged, seed 1, os 1; printed at os 2, where the decimation
 * filter spreads a step and control/ramp-blade1 reads 0.60 dB, UNDER the ceiling: a
 * blind spot stated on every run). control/ramp-both must render what the engine
 * renders on the pad with `mode2` 0; control/ramp-blade1 and control/ramp-blade2 (the
 * ramp on one whole blade only) on that job, and control/ramp-skip on the pad as
 * shipped, must each read over kTickCeil while the engine reads at or under it.
 *
 * LIVENESS row (judged). A silent render reads no hit and no tick. Every spreadRamp 1
 * walk render must be audible and within kLiveDb of its spreadRamp 0 twin in rms.
 *
 * ALSO REQUIRED OF EVERY JOB: the engine knew every key (a misspelt `spreadRamp` would
 * render the present law twice), and no plant refused its job (a refused job renders
 * NaN, and a NaN reading fails its row).
 *
 * THE LAST LINE IS THE VERDICT: `h2_spread_ramp_check: GREEN (...)` or `RED (...)`,
 * with the count of kinds unproven. Exit 0 green, 1 red, 2 the run broke.
 *
 *   node tools/h2_spread_ramp_check.mjs [--bin <h2_harness_render>]
 *
 * Deterministic: no clock, no randomness but the engine's seeded streams. One render
 * process per seed and section, so that no more than a section's samples are held.
 */
import { realpathSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import { jobFor, patch, run } from '../h2/harness/harness.mjs';
import { isolatedJumps } from './h2_frame_hold_check.mjs';

const SR = 48000;
// The host random streams (ADR-216 A4, PROPOSED: eight seeds). The tick measure, the
// plants and the kind rows read seed 1. CHANGING A SEED IS NEVER A FIX.
const kSeeds = [1, 2, 3, 4, 5, 6, 7, 8];
const SEED = 1;
const PAD = 'ledger/breathing-pad';
const C2 = { name: 'C2', note: 36, vel: 0.9 }, G2 = { name: 'G2', note: 43, vel: 0.9 };
const C3 = { name: 'C3', note: 48, vel: 0.9 }, G3 = { name: 'G3', note: 55, vel: 0.9 }, C4 = { name: 'C4', note: 60, vel: 0.8 };
const kWalkNotes = [C2, G2, C3, G3, C4];   // down to C2, where the law's steps are largest (A4 item 7)
const kPadNotes = [C3, G3, C4];            // the printed pad, the frame-hold check's notes
const kSeconds = 8;
const kOsList = [2, 1];
// what every job pins beside `os` and `spreadRamp` (header)
const kPinned = { frameHold: 1, stealFade: 0 };
// the walk job: no noise is drawn, so the spread walks alike with the flag on and off (header)
const kWalk = { mode2: 0, depth2: 0 };
// the printed twin: the pad's noise blade silent (it still draws: NOT the same walk as the pad's)
const kTwin = { depth2: 0 };
// the engine's couple tick, in samples (h2/engine/engine.h kCoupleSamples)
const kTick = 32;
// the last tick position a step of the law can reach, per os (header, AT A TICK BOUNDARY)
const kBoundaryLast = { 1: 1, 2: 6 };
const atBoundary = (os, at) => at % kTick <= kBoundaryLast[os];
// the detector, tools/h2_frame_hold_check.mjs's own numbers (the DETECTOR row holds this copy to it)
const kStepFloor = 0.02, kRatio = 3, kPercentile = 0.99, kWindow = Math.round(0.1 * SR), kFlank = kWindow / 2, kMerge = 480;
// the tick measure's positions: the step lands on position 1, and a second difference
// there is read from positions 0 to 2; the fourth is margin
const kTickFirst = 4;
// PROPOSED, the human's to ratify: the measured spreadRamp 1 worst on the pad (0.50 dB:
// C3, os 2, drift rate 1; 2026-10-13, darwin-arm64) times two.
const kTickCeil = 1.0;
const kTickCases = [['drift rate 0.1 (as shipped)', {}, true], ['drift rate 1', { driftRate: 1 }, true], ['the law off (law 0)', { law: 0 }, false]];
// the composition's job: no noise blade
const kPlantJob = { mode2: 0 };
const kPlantOs = 1, kPlantNote = C3;
// PROPOSED. Measured 2026-10-13: the composition with nothing planted is within 1.4e-15
// of the engine on a bus that peaks near 0.7 (rounding in atanh and tanh).
const kComposeCeil = 1e-12;
// the kinds (control_spread.h Kind), each with its spread amount's key and the value its
// own job gives it, and which the tick measure can hear (measured 2026-10-14; header)
const kKinds = [
  { name: 'position', key: 'bspread', amount: 0.3, heard: true, values: ['cOff'] },
  { name: 'cut', key: 'kspread', amount: 1.5, heard: true, values: ['kAdd', 'kMul'] },
  { name: 'rotation', key: 'rotSpread', amount: 2, heard: false, values: ['rotOff'] },
  { name: 'width', key: 'wspread', amount: 0.3, heard: true, values: ['wMul'] },
  { name: 'depth', key: 'dspread', amount: 0.3, heard: false, values: ['dAdd'] },
  { name: 'index', key: 'ispread', amount: 1, heard: true, values: ['iMul'] },
  { name: 'band', key: 'mspread', amount: 0.3, heard: false, values: ['mr', 'mn'] },
];
// engine.h kSpreadKeys' order within a blade (blade 2's follow, + 9)
const kValueOrder = ['cOff', 'kAdd', 'kMul', 'rotOff', 'wMul', 'dAdd', 'iMul', 'mr', 'mn'];
// every kind at once, for the blade that is not the kind's own, and for the probe
const kEvery = { bspread: 0.3, kspread: 1.5, rotSpread: 0.4, wspread: 0.3, dspread: 0.2, ispread: 0.3, mspread: 0.3 };
const suffixed = (o, s) => Object.fromEntries(Object.entries(o).map(([k, v]) => [k + s, v]));
const kKindBase = { mode2: 0, b2sp: 1, kq: 0, law: 4, driftRate: 12, rot2Follow: 0, w2: 0.3, k2: 5, c2: 0.2 };
const kKindSeconds = 4;
/* the job of one kind on one blade: that blade moves the kind alone, the other moves every kind */
function kindJob(kind, blade) {
  const s = blade === 1 ? '' : '2', o = blade === 1 ? '2' : '';
  const over = { ...kKindBase, ...suffixed(Object.fromEntries(Object.keys(kEvery).map(k => [k, 0])), s), ...suffixed(kEvery, o),
    ['hot' + s]: kind.name === 'band' ? 6 : 2, ['hot' + o]: 6, ['depth' + s]: kind.name === 'depth' ? 0.5 : 1, ['depth' + o]: 0.8, [kind.key + s]: kind.amount };
  if (kind.name === 'index') { over['mode' + s] = 1; over.I = 4; }   // an index is heard on an FM blade
  return over;
}
// the probe's two jobs: every value moves on both blades; a cut is an offset without a rule, a ratio with one
const kProbeBase = { mode2: 0, b2sp: 1, kq: 0, law: 4, driftRate: 1, rot2Follow: 0, hot: 6, hot2: 6, ...kEvery, ...suffixed(kEvery, '2') };
const kProbeJobs = [['no cut rule', {}], ['a cut rule', { kRule: 6, kRule2: 6, kRuleAmt: 0.7, kRuleAmt2: 0.7 }]];
// PROPOSED. Measured 2026-10-14: the engine's worst score over about 36000 member-ticks
// a value is 2.4e-9 (rounding in 32 additions of a share), and an engine without the
// ramp scores 15.00 on every value that moves. control_spread.h kSlots = 64.
const kProbeCeil = 1e-6, kProbeStep = 1, kProbeSlots = 64, kProbeValues = 18;
// liveness: flag on within this many dB of flag off, and neither silent (the frame-hold check's bound)
const kLiveDb = 3;
const kSilentPeak = Math.pow(10, -90 / 20);   // tools/patchspace/metrics.mjs's silence line, -90 dBFS peak

/* B460's measure (header, THE TICK MEASURE): dB, the first positions of the tick over the median position */
export function tickDb(x) {
  const E = new Float64Array(kTick), C = new Float64Array(kTick);
  for (let n = 1; n < x.length - 1; n++) { const d = x[n + 1] - 2 * x[n] + x[n - 1]; E[n % kTick] += d * d; C[n % kTick]++; }
  for (let p = 0; p < kTick; p++) E[p] /= C[p];
  const s = Float64Array.from(E).sort(), med = 0.5 * (s[kTick / 2 - 1] + s[kTick / 2]);
  let first = 0;
  for (let p = 0; p < kTickFirst; p++) if (!(E[p] <= first)) first = E[p];   // a NaN is kept, and fails every bound
  return 10 * Math.log10(first / med);
}
/* the detector's raw hits in one channel, before any merging -> [{ at, step }] (header, JUMP rows) */
export function rawHits(x) {
  const n = x.length, d = new Float64Array(n);
  for (let i = 1; i < n; i++) d[i] = Math.abs(x[i] - x[i - 1]);
  const hits = [];
  for (let a = 0; a < n; a += kWindow) {
    const lo = Math.max(1, a - kFlank), hi = Math.min(n, a + kWindow + kFlank);
    const s = Float64Array.from(d.subarray(lo, hi)).sort();
    const p = s[Math.min(s.length - 1, Math.floor(kPercentile * s.length))];
    for (let i = Math.max(1, a); i < Math.min(n, a + kWindow); i++) if (d[i] > kStepFloor && d[i] > kRatio * p) hits.push({ at: i, step: d[i] });
  }
  return hits;
}
/* raw hits merged by the original's rule -> the sample of each event's largest step */
function mergedAt(hits) {
  const out = [];
  for (const h of hits) {
    const l = out[out.length - 1];
    if (l && h.at - l.last <= kMerge) { l.last = h.at; if (h.step > l.step) { l.at = h.at; l.step = h.step; } } else out.push({ ...h, last: h.at });
  }
  return out.map(e => e.at);
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

const job = (seed, n, os, over, engine, seconds = kSeconds, pre = true) => jobFor({ patch: PAD, engine, sr: SR, seed, seconds, notes: [n.note], vel: n.vel, pre,
  overrides: { os, ...kPinned, ...over } });

export function main(argv) {
  const bi = argv.indexOf('--bin'), opt = bi >= 0 ? { bin: argv[bi + 1] } : {};
  let red = 0, jobsN = 0, unknownN = 0, firstUnknown = '';
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(8)} ${text}`); if (!ok) red++; };
  const note = (tag, text) => console.log(`NOTE  ${tag.padEnd(8)} ${text}`);
  patch(PAD);   // throws if the pad has left the reference set
  /* one render process; the results by key */
  const batch = entries => {
    const rs = run(entries.map(e => e[1]), opt), out = {};
    entries.forEach((e, i) => { out[e[0]] = rs[i]; });
    jobsN += rs.length;
    for (const r of rs) if (r.summary.unknownKeys) { unknownN++; firstUnknown = firstUnknown || r.name; }
    return out;
  };

  // ---- the renders, a seed at a time; only the readings are kept
  const walk = {};          // `${os}/${sr}` -> [{ seed, note, hits }] (raw); 'skip' -> the plant's
  const printed = {};       // `${kind}/${os}` -> [{ seed, note, jumps }], kind pad | twin, spreadRamp 1
  const probes = [];        // [{ name, far, members }]
  const live = [];
  let detectorBad = 0, detectorN = 0, firstDetector = '';
  const tick = {}, diff = {};
  for (const seed of kSeeds) {
    // the walk job: judged
    const wj = [];
    for (const os of kOsList) for (const n of kWalkNotes) for (const sr of [1, 0]) wj.push([`${os}/${n.name}/${sr}`, job(seed, n, os, { ...kWalk, spreadRamp: sr })]);
    const W = batch(wj);
    for (const os of kOsList) for (const n of kWalkNotes) {
      for (const sr of [1, 0]) {
        const r = W[`${os}/${n.name}/${sr}`], hits = rawHits(r.preL);
        (walk[`${os}/${sr}`] ||= []).push({ seed, note: n.name, hits });
        // the copy against the original
        const a = mergedAt(hits), b = isolatedJumps(r.preL).map(e => e.at);
        detectorN++;
        if (a.length !== b.length || a.some((v, i) => v !== b[i])) { detectorBad++; firstDetector = firstDetector || `seed ${seed} os ${os} ${n.name} spreadRamp ${sr}: ${a.length} against ${b.length}`; }
      }
      live.push([`seed ${seed} os ${os} ${n.name}`, level(W[`${os}/${n.name}/1`]), level(W[`${os}/${n.name}/0`])]);
    }
    // the walk probe, the skipped-tick plant, and the printed pad and twin
    const pj = [];
    for (const os of kOsList) for (const n of kWalkNotes) if (os === 1 || seed === SEED) pj.push([`probe/${os}/${n.name}`, job(seed, n, os, { ...kWalk, spreadRamp: 1 }, 'control/walk-probe', kSeconds, false)]);
    for (const n of kWalkNotes) pj.push([`skip/${n.name}`, job(seed, n, kPlantOs, { ...kWalk, spreadRamp: 1 }, 'control/ramp-skip')]);
    for (const os of kOsList) for (const n of kPadNotes) {
      pj.push([`pad/${os}/${n.name}`, job(seed, n, os, { spreadRamp: 1 })]);
      pj.push([`twin/${os}/${n.name}`, job(seed, n, os, { ...kTwin, spreadRamp: 1 })]);
    }
    const P = batch(pj);
    for (const [key, r] of Object.entries(P)) {
      const [kind, a, b] = key.split('/');
      if (kind === 'probe') { let far = 0, members = Infinity; for (let i = 0; i < r.L.length; i++) { if (!(r.L[i] <= far)) far = r.L[i]; if (!(r.R[i] >= members)) members = r.R[i]; } probes.push({ name: `seed ${seed} os ${a} ${b}`, far, members }); }
      else if (kind === 'skip') (walk.skip ||= []).push({ seed, note: a, hits: rawHits(r.preL) });
      else (printed[`${kind}/${a}`] ||= []).push({ seed, note: b, jumps: isolatedJumps(r.preL) });
    }
    if (seed === SEED) {
      for (const os of kOsList) for (const n of kPadNotes) tick[`pad/${os}/${n.name}/1`] = tickDb(P[`pad/${os}/${n.name}`].preL);
      const tj = [];
      for (const os of kOsList) {
        for (const n of kPadNotes) tj.push([`pad/${os}/${n.name}/0`, job(seed, n, os, { spreadRamp: 0 })]);
        for (const [name, over] of kTickCases.slice(1)) for (const sr of [1, 0]) tj.push([`tick/${os}/${name}/${sr}`, job(seed, C3, os, { ...over, spreadRamp: sr })]);
        for (const sr of [1, 0]) {
          tj.push([`plant/${os}/engine/${sr}`, job(seed, kPlantNote, os, { ...kPlantJob, spreadRamp: sr })]);
          tj.push([`plant/${os}/both/${sr}`, job(seed, kPlantNote, os, { ...kPlantJob, spreadRamp: sr }, 'control/ramp-both')]);
        }
        tj.push([`plant/${os}/blade1`, job(seed, kPlantNote, os, { ...kPlantJob, spreadRamp: 1 }, 'control/ramp-blade1')]);
        tj.push([`plant/${os}/blade2`, job(seed, kPlantNote, os, { ...kPlantJob, spreadRamp: 1 }, 'control/ramp-blade2')]);
        tj.push([`plant/${os}/skip`, job(seed, kPlantNote, os, { spreadRamp: 1 }, 'control/ramp-skip')]);
      }
      const T = batch(tj);
      for (const [key, r] of Object.entries(T)) tick[key] = tickDb(r.preL);
      for (const os of kOsList) for (const sr of [1, 0]) diff[`${os}/${sr}`] = maxDiffPre(T[`plant/${os}/both/${sr}`], T[`plant/${os}/engine/${sr}`]);
    }
  }
  // the pad as shipped through the walk probe: its control
  const padProbe = batch([['p', job(SEED, C3, 1, { spreadRamp: 1 }, 'control/walk-probe', kSeconds, false)]]).p;
  let padFar = 0; for (let i = 0; i < padProbe.L.length; i++) if (!(padProbe.L[i] <= padFar)) padFar = padProbe.L[i];
  // every kind: the sound (seed 1, os 1) and the values themselves
  const kj = [];
  for (const kind of kKinds) for (const blade of [1, 2]) {
    const over = { ...kindJob(kind, blade), spreadRamp: 1 }, k = `${kind.name}/${blade}`;
    kj.push([`${k}/engine`, job(SEED, C3, kPlantOs, over, undefined, kKindSeconds)]);
    kj.push([`${k}/both`, job(SEED, C3, kPlantOs, over, 'control/ramp-both', kKindSeconds)]);
    kj.push([`${k}/plant`, job(SEED, C3, kPlantOs, over, `control/step-${kind.name}-b${blade}`, kKindSeconds)]);
  }
  const K = batch(kj);
  const PB = batch(kProbeJobs.map(([name, over]) => [name, job(SEED, C3, kPlantOs, { ...kProbeBase, ...over, spreadRamp: 1 }, 'control/ramp-probe', kKindSeconds, false)]));

  row(!unknownN, 'KEYS', `the engine knew every key on ${jobsN - unknownN} of ${jobsN} jobs${unknownN ? `; first refused on: ${firstUnknown}` : ''}`);
  console.log(`${PAD}; pinned on every job beside os and spreadRamp: ${JSON.stringify(kPinned)}; ${kSeconds} s at ${SR} Hz, seeds ${kSeeds[0]} to ${kSeeds[kSeeds.length - 1]}, left pre-limiter bus`);
  console.log(`the walk job: the pad with ${JSON.stringify(kWalk)}; notes ${kWalkNotes.map(n => n.name).join(' ')}; at a tick boundary: tick positions 0 to ${kBoundaryLast[1]} at os 1 and 0 to ${kBoundaryLast[2]} at os 2 (header)`);

  // ---- the walk is the same with the flag on and off
  {
    const bad = probes.filter(p => !(p.far === 0 && p.members > 0));
    row(bad.length === 0 && probes.length === kWalkNotes.length * (kSeeds.length + 1), 'WALK',
      `control/walk-probe on ${probes.length} walk jobs (every note and seed at os 1; seed ${SEED}'s notes at os 2): the spread slots and phases of an engine with the ramp and one without differ by exactly 0 on ${probes.length - bad.length}, with ${Math.min(...probes.map(p => p.members))} members compared at the least; required all, with members compared${bad.length ? `; first to differ: ${bad[0].name}, by ${bad[0].far}` : ''}`);
    row(padFar > 0, 'WALK', `its control, the pad AS SHIPPED (seed ${SEED}, C3, os 1): the two engines' walks differ by ${padFar.toPrecision(3)}; required more than 0 (the noise blade's draws move the walk with the flag)`);
  }

  // ---- the judged rows: raw hits at a tick boundary on the walk job
  const flat = key => (walk[key] || []).flatMap(c => c.hits.map(e => ({ ...e, seed: c.seed, note: c.note })));
  const one = e => `${e.step.toFixed(3)} at ${e.at} (seed ${e.seed}, ${e.note}, tick position ${e.at % kTick})`;
  const list = (js, cap = 12) => (js.length ? `${js.length}: ${js.slice(0, cap).map(one).join('; ')}${js.length > cap ? `; and ${js.length - cap} more` : ''}` : '0');
  const rendersWant = kSeeds.length * kWalkNotes.length;
  row(detectorBad === 0 && detectorN === 2 * 2 * rendersWant, 'DETECTOR', `this check's copy of the frame-hold check's detector, merged by that check's rule, names that check's events on ${detectorN - detectorBad} of ${detectorN} walk renders; required all${firstDetector ? `; first to differ: ${firstDetector}` : ''}`);
  for (const os of kOsList) {
    const on = flat(`${os}/1`), off = flat(`${os}/0`), onB = on.filter(e => atBoundary(os, e.at)), offB = off.filter(e => atBoundary(os, e.at));
    const n = (walk[`${os}/1`] || []).length;
    row(onB.length === 0 && n === rendersWant, 'JUMP', `os ${os}, the walk job, spreadRamp 1, ${n} renders: raw detector hits at a tick boundary ${list(onB)}; required 0 (${on.length} raw hit(s) in all, none of them judged: off the boundary a hit is the wave's own)`);
    row(offB.length > 0, 'CONTROL', `os ${os}, the same renders with spreadRamp 0 (the present law): raw detector hits at a tick boundary ${offB.length}, of ${off.length} in all; required at least 1`);
    for (const nn of kWalkNotes) {
      const c = k => flat(k).filter(e => e.note === nn.name);
      note('JUMP', `os ${os} ${nn.name} (the row above, by note): raw hits at a tick boundary / in all: spreadRamp 1 ${c(`${os}/1`).filter(e => atBoundary(os, e.at)).length} / ${c(`${os}/1`).length}; spreadRamp 0 ${c(`${os}/0`).filter(e => atBoundary(os, e.at)).length} / ${c(`${os}/0`).length}`);
    }
  }
  {
    const os = kPlantOs, js = flat('skip'), b = js.filter(e => atBoundary(os, e.at));
    row(b.length > 0, 'SKIP', `os ${os}, control/ramp-skip (the ramp dropped at one tick in four) on the ${(walk.skip || []).length} walk jobs of that os, read as the JUMP row reads the engine: raw detector hits at a tick boundary ${b.length}, of ${js.length} in all; required at least 1, or the JUMP row could not see a step the ramp leaves`);
  }

  // ---- printed, not judged: the pad as shipped and its noise-silent twin (ADR-216 A4 item 8)
  console.log(`printed, NOT judged (PROPOSED, ADR-216 A4 item 8): the pad as shipped and its twin with the noise blade silent (${JSON.stringify(kTwin)}), spreadRamp 1, ${kPadNotes.map(n => n.name).join(' ')}, merged events as the frame-hold check counts them`);
  const pflat = key => (printed[key] || []).flatMap(c => c.jumps.map(e => ({ ...e, seed: c.seed, note: c.note })));
  for (const os of kOsList) {
    const pad = pflat(`pad/${os}`), twin = pflat(`twin/${os}`);
    note('PAD', `os ${os}, the pad as shipped, ${(printed[`pad/${os}`] || []).length} renders: ${pad.length} isolated jump(s). ROADMAP B460's first added acceptance asked for 0 here; it is ${pad.length ? 'NOT met' : 'met'} at this os on these seeds`);
    for (const e of pad) note('ATTRIB', `os ${os}: ${one(e)} is ${atBoundary(os, e.at) ? 'AT a tick boundary (by chance or by the law: this line cannot tell)' : 'not at a tick boundary, so not a step of the law'}; ` +
      `the twin of that render reads ${list(twin.filter(t => t.seed === e.seed && t.note === e.note))} (a silent blade still draws, so at the same flag the twin walks the pad's own spread)`);
    note('TWIN', `os ${os}, the twin, ${(printed[`twin/${os}`] || []).length} renders: isolated jumps ${list(twin)}`);
  }

  // ---- B460's own measure (seed 1)
  console.log(`tick energy (seed ${SEED}): the second difference's mean square at the first ${kTickFirst} positions of the ${kTick}-sample tick (the largest) over the median position; ceiling ${dB(kTickCeil)}, PROPOSED (the human's to ratify)`);
  for (const os of kOsList) {
    for (const [name, , judged] of kTickCases) {
      const key = sr => (name === kTickCases[0][0] ? `pad/${os}/C3/${sr}` : `tick/${os}/${name}/${sr}`);
      const on = tick[key(1)], off = tick[key(0)];
      if (!judged) { note('TICK', `os ${os} C3, ${name} (not judged: the floor of the measure): spreadRamp 1 reads ${dB(on)}, spreadRamp 0 reads ${dB(off)}`); continue; }
      row(on <= kTickCeil, 'TICK', `os ${os} C3, ${name}, spreadRamp 1: ${dB(on)}; required at or under ${dB(kTickCeil)} (PROPOSED)`);
      row(off > kTickCeil, 'TICK', `os ${os} C3, ${name}, spreadRamp 0 (its control): ${dB(off)}; required over ${dB(kTickCeil)}`);
    }
    for (const n of kPadNotes.slice(1)) note('TICK', `os ${os} ${n.name}, ${kTickCases[0][0]} (not judged): spreadRamp 1 reads ${dB(tick[`pad/${os}/${n.name}/1`])}, spreadRamp 0 reads ${dB(tick[`pad/${os}/${n.name}/0`])}`);
  }

  // ---- the whole-blade plants (seed 1)
  console.log(`plants (h2/harness/control_spread.h), one held ${kPlantNote.name} at seed ${SEED}, read with the tick measure; judged at os ${kPlantOs}; the composition's job is the pad with ${JSON.stringify(kPlantJob)} (no noise blade)`);
  for (const os of kOsList) {
    const judged = os === kPlantOs;
    for (const sr of [1, 0]) {
      const d = diff[`${os}/${sr}`];
      const text = `os ${os}, spreadRamp ${sr}: control/ramp-both (nothing planted) against the engine on the same job: max |difference| ${d.toExponential(2)} on the pre-limiter bus`;
      if (judged) row(d <= kComposeCeil, 'COMPOSE', `${text}; required at or under ${kComposeCeil} (PROPOSED)`); else note('COMPOSE', `${text} (not judged at this os)`);
    }
    const eng = tick[`plant/${os}/engine/1`], pad = tick[`pad/${os}/${kPlantNote.name}/1`];
    const plants = [['control/ramp-blade1', 'the ramp reaches blade 1 only', 'blade1', eng, 'the engine on the same job'],
      ['control/ramp-blade2', 'the ramp reaches blade 2 only', 'blade2', eng, 'the engine on the same job'],
      ['control/ramp-skip', 'the ramp dropped at one tick in four', 'skip', pad, 'the engine on the same job, the pad as shipped']];
    if (judged) row(eng <= kTickCeil, 'PLANT', `os ${os}, the engine on the composition's job, spreadRamp 1: ${dB(eng)}; required at or under ${dB(kTickCeil)} (PROPOSED)`);
    for (const [id, what, key, ref, refName] of plants) {
      const v = tick[`plant/${os}/${key}`];
      const text = `os ${os}, ${id} (${what}): ${dB(v)}; ${refName} reads ${dB(ref)}`;
      if (judged) row(v > kTickCeil, 'PLANT', `${text}; the plant is required over ${dB(kTickCeil)}, or this check cannot see that fault`);
      else note('PLANT', `${text} (not judged at this os: the decimation filter spreads a step over several samples${v > kTickCeil ? '' : `, and this measure does NOT see this plant here: it reads under the ceiling of ${dB(kTickCeil)}`})`);
    }
  }

  // ---- every kind of value: the values themselves (the probe)
  const proven = new Set();
  console.log(`every kind, the values themselves: control/ramp-probe, one held C3 at seed ${SEED}, os ${kPlantOs}, ${kKindSeconds} s, on a job that moves every value of both blades (${kProbeJobs.map(j => j[0]).join('; ')}); a value's score over a tick is 0 for equal shares and 15 for a step; ceiling ${kProbeCeil}, PROPOSED`);
  const readProbe = r => {
    const eng = new Array(kProbeValues).fill(NaN), plant = new Array(kProbeValues).fill(NaN), moved = new Array(kProbeValues).fill(0);
    for (let i = r.L.length - kProbeSlots; i < r.L.length; i++) {
      const s = r.R[i];
      if (s < kProbeValues) eng[s] = r.L[i]; else if (s < 2 * kProbeValues) plant[s - kProbeValues] = r.L[i]; else if (s < 3 * kProbeValues) moved[s - 2 * kProbeValues] = r.L[i];
    }
    return { eng, plant, moved };
  };
  const probeRead = kProbeJobs.map(([name]) => ({ name, ...readProbe(PB[name]) }));
  let unprovenValues = 0;
  for (const kind of kKinds) for (const blade of [1, 2]) {
    const parts = [];
    let ok = true, any = false;
    for (const v of kind.values) {
      const idx = kValueOrder.indexOf(v) + (blade === 2 ? 9 : 0), name = v + (blade === 2 ? '2' : '');
      const from = probeRead.filter(p => p.moved[idx] > 0);
      if (!from.length) { unprovenValues++; parts.push(`${name} moved on neither job`); console.log(`UNPROVEN ${kind.name} (blade ${blade}): ${name} moved on neither probe job, so nothing is shown of it`); continue; }
      any = true;
      const eng = Math.max(...from.map(p => p.eng[idx])), plant = Math.min(...from.map(p => p.plant[idx])), ticks = from.reduce((a, p) => a + p.moved[idx], 0);
      if (!(eng <= kProbeCeil && plant >= kProbeStep)) ok = false;
      parts.push(`${name} (${from.map(p => p.name).join(', ')}; ${ticks} member-ticks): the engine ${eng.toExponential(1)}, the plant ${plant.toFixed(2)}`);
    }
    if (any) row(ok, 'PROBE', `${kind.name}, blade ${blade}: ${parts.join('; ')}; required: the engine at or under ${kProbeCeil}, and the plant (this kind's values from the engine without the ramp: the kind left stepped) at least ${kProbeStep}`);
    if (any && ok) proven.add(`${kind.name}/${blade}`);
  }

  // ---- every kind of value: the sound (the tick measure and the one-kind plants)
  console.log(`every kind, the sound: one held C3 at seed ${SEED}, os ${kPlantOs}, ${kKindSeconds} s, drift rate ${kKindBase.driftRate}; one blade moves the kind alone, the other every kind; control/step-<kind>-b<n> steps the one blade's values; the tick measure, ceiling ${dB(kTickCeil)} (PROPOSED)`);
  for (const kind of kKinds) for (const blade of [1, 2]) {
    const k = `${kind.name}/${blade}`, eng = tickDb(K[`${k}/engine`].preL), plant = tickDb(K[`${k}/plant`].preL), d = maxDiffPre(K[`${k}/both`], K[`${k}/engine`]);
    const text = `${kind.name}, blade ${blade} (${kind.key}${blade === 2 ? '2' : ''} ${kind.amount}): the engine ${dB(eng)}; control/step-${kind.name}-b${blade} ${dB(plant)}; the composition with nothing planted within ${d.toExponential(1)} of the engine`;
    if (kind.heard) row(eng <= kTickCeil && plant > kTickCeil && d <= kComposeCeil, 'KIND', `${text}; required: the engine at or under ${dB(kTickCeil)}, the plant over it, the composition within ${kComposeCeil}`);
    else {
      row(Number.isFinite(plant) && d <= kComposeCeil, 'KIND', `${text}; required here only that the plant took the job and the composition is within ${kComposeCeil}`);
      note('KIND', `${kind.name}, blade ${blade} (NOT judged on the sound): the tick measure does not tell this plant (${dB(plant)}) from the engine (${dB(eng)}); this kind rests on its PROBE row` +
        `${eng > kTickCeil ? `. THE ENGINE ITSELF, RAMP ON, IS OVER THE PROPOSED CEILING ON THIS JOB (${dB(eng)} against ${dB(kTickCeil)}): reported, not judged (header)` : ''}`);
    }
  }
  const unproven = kKinds.flatMap(kind => [1, 2].filter(b => !proven.has(`${kind.name}/${b}`)).map(b => `${kind.name} (blade ${b})`));
  for (const u of unproven) console.log(`UNPROVEN ${u}: no judged row shows that this kind ramps`);

  // ---- liveness: every spreadRamp 1 walk render audible, and as loud as its spreadRamp 0 twin
  let liveBad = 0, liveWorst = 0, liveWorstName = '', firstBad = '';
  for (const [name, a, b] of live) {
    const db = 20 * Math.log10(a.rms / b.rms);
    if (!(a.peak >= kSilentPeak && b.peak >= kSilentPeak && Math.abs(db) <= kLiveDb)) { liveBad++; firstBad = firstBad || `${name}: rms ${a.rms.toExponential(2)} against ${b.rms.toExponential(2)} (${db.toFixed(2)} dB)`; }
    if (Math.abs(db) > liveWorst) { liveWorst = Math.abs(db); liveWorstName = name; }
  }
  row(liveBad === 0 && live.length > 0, 'LIVENESS', `${live.length - liveBad} of ${live.length} spreadRamp 1 walk renders are audible and within ${kLiveDb} dB of their spreadRamp 0 twin in rms (largest ${liveWorst.toFixed(2)} dB, ${liveWorstName})` +
    `${firstBad ? `; first failure: ${firstBad}` : ''}`);

  console.log(`h2_spread_ramp_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; ${unproven.length} kind(s) unproven${unproven.length ? ': ' + unproven.join(', ') : ''}; ` +
    `the walk job at a tick boundary over ${kSeeds.length} seeds, ${kWalkNotes.length} notes and os ${kOsList.join(' and ')}, its walk, its controls, ` +
    `every kind of value by the probe and ${kKinds.filter(k => k.heard).length} of ${kKinds.length} kinds on the sound, the tick measure, the plants, liveness; the pad as shipped printed, not judged)`);
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
