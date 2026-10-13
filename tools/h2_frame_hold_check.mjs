/*
 * h2_frame_hold_check.mjs — ADR-216 D6 (`frameHold`, ROADMAP B461) does what it was
 * ruled to do, on the C++ engine: with the flag on, a held note of the pad the human
 * heard click reads no isolated jump of the frame's making; with it off (the present
 * law) it reads them; and where the swarm is locked the flag changes nothing above a
 * stated ceiling.
 *
 * WIRED: ./verify full
 *
 * THE FAULT (B461). With `frame` on, each member's blade sits at c + lead, and lead
 * is the member's phase measured from psi, the angle of the summed phase vectors.
 * The engine recomputes psi and every lead once per 32 samples. Where the members
 * balance, the order parameter R passes through zero, psi is undefined and swings
 * by up to half a cycle in ONE tick, and every blade jumps at once: a click inside
 * a held note. D6 gives each voice a frame that holds its course there, and hands
 * each lead to its blade as a ramp (h2/engine/engine.h, holdFrame).
 *
 * WHAT IS JUDGED, AND WHO RULED IT. The human, 2026-10-12, on the lead's
 * recommendation (ROADMAP B461's note; to be recorded by the lead as ADR-216 A2):
 * "judge the fix with Position spread at 0, at both 1x and 2x oversampling. The
 * present law must still read at least one jump there, so the test can see the
 * fault. The pad-as-shipped row is printed but not judged, and becomes B460's test."
 *
 * WHAT IS RENDERED. Everything goes through the engine harness (h2/harness/), so
 * this is the C++ engine as it ships, never the JS one. One held note of
 * `ledger/breathing-pad`, 48 kHz, 8 s, host seed 1, at C3 (velocity 0.9), G3 (0.9)
 * and C4 (0.8), at os 2 (the engine's own default, the parity set's rate) and at
 * os 1 (the device default since B445, and what the preview plays). An ISOLATED
 * JUMP is a sample-to-sample step on the left channel of the pre-limiter bus that
 * is BOTH over 0.02 AND over 3 times the 99th percentile of the steps around it:
 * the percentile is taken per 100 ms window (4800 samples) over the window and
 * 2400 samples either side, and events within 480 samples are one.
 *
 *   JUMP rows (judged). The pad with Position spread 0 (`bspread` 0) and
 *   frameHold 1. Required: no isolated jump, on every one of the six renders
 *   (three notes, two os).
 *
 *   CONTROL rows (judged; LIBRARY L0032: a detector that cannot see the fault proves
 *   nothing). The same six renders with frameHold 0, the present law. Required: at
 *   least one isolated jump at C3 AND at C4, at each os, or this check is RED
 *   whatever the flag-on rows read. G3 is printed and not required. (Measured
 *   2026-10-12: 3 / 1 / 2 at os 2 and 5 / 2 / 4 at os 1.)
 *
 *   WHY POSITION SPREAD 0. The same detector also sees steps that are not the
 *   frame's: ROADMAP B460, the spread law's values reaching the blades unsmoothed
 *   every 32 samples. On this pad the drifting law (law 4) walks each blade's
 *   POSITION offset by `bspread` times a random step per tick, and at C3 such a
 *   step is the size of the phase's own advance per sample. `bspread` 0 closes
 *   that one route and leaves the frame's. The critic's seed sweep (2026-10-12) is
 *   the evidence that what is left on the pad as shipped is not the frame's: the
 *   residue moves with the host seed, which only the spread's drift draws from;
 *   all 28 hits over 18 seeds fall on sample 1 of a tick; and with `bspread` 0 the
 *   flag-on render reads 0 on 18 of 18 seeds.
 *
 *   B460 rows (NOTE lines, NOT judged). The pad AS SHIPPED, without the `bspread`
 *   override, flag on and flag off, at both os. They belong to ROADMAP B460: its
 *   acceptance promotes the flag-on line to a judged row requiring 0 / 0 / 0.
 *   They are printed so that the reading stays on the record every run. Where a
 *   flag-on line reads a jump, an ATTRIB line also prints the pad with the frame
 *   off altogether (`frame` 0): whatever that reads, the frame did not make.
 *
 *   THE PAD'S SETTINGS ARE PINNED HERE, NOT LEFT TO THE BANK (the lead's amendment to
 *   this check's brief, 2026-10-12, when ADR-217 ordered an edit of the bank's
 *   Breathing pad to detune 0 and K -0.3). The click needs the members to drift
 *   apart and re-form, so it depends on the patch: `detune` 14 and `K` 0.45 (with
 *   detune 0 the members stay locked and there is nothing to reproduce), a frame
 *   that follows the swarm (`frame` 1), settled start phases (`phaseMode` 2), six
 *   members (`N` 6) and the drifting spread law (`law` 4 at `driftRate` 0.1). Those
 *   seven are the values under which the lead measured the click. They are written
 *   over the reference patch on every pad job, so an edit of the bank's pad cannot
 *   quietly blind the control: the rest of the patch is still the bank's, and if a
 *   later edit there removes the click the CONTROL rows turn this check red.
 *
 *   LIVENESS rows (judged). A silent render reads zero jumps and zero difference. So
 *   every flag-on render must be audible and within kLiveDb of its flag-off twin in
 *   rms, the pad rows and the locked rows alike. The one patch the reference set
 *   declares silent (`expect: silent`) must be silent, and is counted nowhere.
 *
 *   LOCKED rows (acceptance 3). Every reference patch in h2/harness/patches.json,
 *   one held C3, 4 s, the frame on (`frame` 1) and six members (`N` 6, so that D6 is
 *   engaged in every patch: with one member there is no lead), flag on against flag
 *   off, largest |difference| over both output channels. Two cases:
 *     UNISON  kUnison: one frequency, one start phase, no coupling, no onset burst,
 *             no swarm drift, no member waiting. Every lead is zero. This shows only
 *             that D6 leaves a unison alone. Judged over the whole render.
 *     COUPLED kCoupled: the same with `detune` 14 and `K` 1: the members lock at
 *             DISTINCT lags, so the leads are not zero and the frame has a centre to
 *             follow. The leads slide into place over the first seconds, and there
 *             the ramp differs from the step BY DESIGN; so it is judged from
 *             kCoupledFrom seconds on, and the earlier window's difference is
 *             printed, unjudged, so the ramp's effect on a lock that is still
 *             forming is on the record.
 *   With the swarm locked the frame IS the measured centre bit for bit (holdFrame),
 *   so the leads' targets are the present law's and only their last-bit wobble is
 *   ramped instead of stepped.
 *   The ceilings are MEASURED, not chosen to pass (2026-10-12, darwin-arm64, the
 *   worst judged row of each case, then a stated margin): see kUnisonCeil and
 *   kCoupledCeil below.
 *   A patch that cannot show a difference that small is NOT JUDGED, on evidence
 *   re-measured every run (ADR-065's rule): each patch is also rendered against
 *   ITSELF, flag off, its note one ULP sharp. If that alone moves the output by
 *   more than the ceiling, the patch amplifies a last-bit change. Only the two
 *   patches named in kMaySkip (feedback and cross-modulation at their maxima) may
 *   be excused that way; any other patch that fails the test is RED. At least
 *   kLockMinJudged patches must be judged in each case.
 *   One of those two is not even locked in the COUPLED case: `corner/continuous-max`
 *   carries `spread` 24, which multiplies the detune, and a scratch probe read R
 *   down to 0.005 and 53 hold exits in its 4 s. Its difference there (about 1.4) is
 *   D6 at work on an unlocked swarm, which is what D6 is for, and says nothing
 *   about a lock. The settings are the lead's (the unison case plus detune 14 and
 *   K 1) and are not widened here to force it.
 *
 * ALSO REQUIRED OF EVERY JOB: the engine knew every key (a misspelt `frameHold`
 * would render the present law twice and read as "no difference").
 *
 * THE LAST LINE IS THE VERDICT: `h2_frame_hold_check: GREEN (...)` or `RED (...)`.
 * Exit 0 green, 1 red, 2 the run broke (no binary, a refused job).
 *
 *   node tools/h2_frame_hold_check.mjs [--bin <h2_harness_render>]
 *
 * Deterministic: no clock, no randomness but the engine's seeded streams.
 */
import { realpathSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import { jobFor, loadPatches, mtof, patch, patchCmds, run } from '../h2/harness/harness.mjs';

const SR = 48000;
// The host random stream. The literal JUMP C3 row reads 0 on some host seeds (3, 8, 9,
// 13 and 17 of 1 to 18, the critic's count, 2026-10-12): whether B460's step falls
// inside the 8 seconds depends on the trajectory. CHANGING THE SEED IS NEVER A FIX.
const SEED = 1;
const PAD = 'ledger/breathing-pad';
// The seven settings the click depends on, as the lead measured it (header).
export const kPadPinned = { frame: 1, phaseMode: 2, N: 6, law: 4, driftRate: 0.1, detune: 14, K: 0.45 };
const NOTES = [{ name: 'C3', note: 48, vel: 0.9 }, { name: 'G3', note: 55, vel: 0.9 }, { name: 'C4', note: 60, vel: 0.8 }];
const kJumpSeconds = 8;
const kControlMustFire = ['C3', 'C4'];
const kOsList = [2, 1];   // both judged (header)
// the detector (header): a step over kStepFloor AND over kRatio x the local 99th percentile
const kStepFloor = 0.02, kRatio = 3, kPercentile = 0.99;
const kWindow = Math.round(0.1 * SR), kFlank = kWindow / 2, kMerge = 480;
// liveness (header): flag on within this many dB of flag off, and neither silent.
// Measured 2026-10-12: the largest |difference| over the 30 pairs was 0.56 dB (the pad
// at os 1, C3), so 3 dB is about five times that.
const kLiveDb = 3;
const kSilentPeak = Math.pow(10, -90 / 20);   // tools/patchspace/metrics.mjs's silence line, -90 dBFS peak
// the locked cases (header)
export const kUnison = { frame: 1, N: 6, phaseMode: 2, detune: 0, K: 0, onset: 0, driftDepth: 0, onsetScatter: 0 };
export const kCoupled = { ...kUnison, detune: 14, K: 1 };
const kLockNote = 48, kLockVel = 0.9, kLockSeconds = 4, kCoupledFrom = 2;
// Measured worst 2026-10-12 (darwin-arm64): unison 7.3e-14 (ledger/defaults and
// ledger/quarter-sync); coupled, from 2 s on, 1.5e-11 (ledger/fold-over-sync; the
// critic read up to 1.3e-11). Each ceiling is about 40 times its measured worst, and
// both are three orders or more under the parity bound (1e-6).
const kUnisonCeil = 3e-12, kCoupledCeil = 6e-10;
const kMaySkip = ['corner/continuous-max', 'corner/continuous-max-toggles-on'];
const kLockMinJudged = 5;

/* isolated jumps in one channel -> [{ at, step, ratio }], merged (header) */
export function isolatedJumps(x) {
  const n = x.length, d = new Float64Array(n);
  for (let i = 1; i < n; i++) d[i] = Math.abs(x[i] - x[i - 1]);
  const hits = [];
  for (let a = 0; a < n; a += kWindow) {
    const lo = Math.max(1, a - kFlank), hi = Math.min(n, a + kWindow + kFlank);
    const s = Float64Array.from(d.subarray(lo, hi)).sort();
    const p = s[Math.min(s.length - 1, Math.floor(kPercentile * s.length))];
    for (let i = Math.max(1, a); i < Math.min(n, a + kWindow); i++)
      if (d[i] > kStepFloor && d[i] > kRatio * p) hits.push({ at: i, step: d[i], ratio: d[i] / p });
  }
  const out = [];
  for (const h of hits) {
    const l = out[out.length - 1];
    if (l && h.at - l.last <= kMerge) { l.last = h.at; if (h.step > l.step) { l.at = h.at; l.step = h.step; l.ratio = h.ratio; } }
    else out.push({ ...h, last: h.at });
  }
  return out;
}
const describe = j => (j.length ? `${j.length} (${j.map(e => `${e.step.toFixed(3)} at ${e.at}, ${e.ratio.toFixed(1)}x`).join('; ')})` : '0');
const nextUp = x => { const f = new Float64Array([x]), b = new BigUint64Array(f.buffer); b[0] += 1n; return f[0]; };
/* largest |a - b| over both output channels, frames [from, to) */
function maxDiff(a, b, from = 0, to = a.L.length) {
  let m = 0;
  for (let i = from; i < to; i++) {
    const l = Math.abs(a.L[i] - b.L[i]), r = Math.abs(a.R[i] - b.R[i]);
    if (!(l <= m)) m = l;   // a NaN difference is kept, and fails every bound
    if (!(r <= m)) m = r;
  }
  return m;
}
/* a render's level over both output channels: { rms, peak } */
function level(r) {
  let e = 0, pk = 0;
  for (let i = 0; i < r.L.length; i++) { e += r.L[i] * r.L[i] + r.R[i] * r.R[i]; pk = Math.max(pk, Math.abs(r.L[i]), Math.abs(r.R[i])); }
  return { rms: Math.sqrt(e / (2 * r.L.length)), peak: pk };
}

const padJob = (n, over) => jobFor({ patch: PAD, sr: SR, seed: SEED, seconds: kJumpSeconds, notes: [n.note], vel: n.vel, pre: true,
  overrides: { ...kPadPinned, ...over } });
const lockJob = (p, set, flag, freq) => jobFor({ patch: p, sr: SR, seed: SEED,
  cmds: [...patchCmds(p, { ...set, frameHold: flag }), ['on', kLockNote, freq, kLockVel], ['render', kLockSeconds * SR]] });

export function main(argv) {
  const bi = argv.indexOf('--bin'), opt = bi >= 0 ? { bin: argv[bi + 1] } : {};
  let red = 0;
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(8)} ${text}`); if (!ok) red++; };
  const note = (tag, text) => console.log(`NOTE  ${tag.padEnd(8)} ${text}`);
  const patches = loadPatches().patches;
  patch(PAD);   // throws if the pad has left the reference set

  // One process renders everything. The pad: per os, per note, the flag on and off, as
  // pinned and with Position spread 0. The locked cases: per patch, unison and coupled,
  // the flag on, off, and off with the note one ULP sharp.
  const jobs = [], at = {};
  const add = (key, job) => { at[key] = jobs.length; jobs.push(job); };
  for (const os of kOsList) for (const n of NOTES) for (const sp of ['pad', 'bs0']) for (const fh of [1, 0])
    add(`${os}/${n.name}/${sp}/${fh}`, padJob(n, { os, frameHold: fh, ...(sp === 'bs0' ? { bspread: 0 } : {}) }));
  const f0 = mtof(kLockNote);
  const CASES = [['unison', kUnison], ['coupled', kCoupled]];
  for (const p of patches) for (const [c, set] of CASES) {
    add(`${p.id}/${c}/1`, lockJob(p, set, 1, f0));
    add(`${p.id}/${c}/0`, lockJob(p, set, 0, f0));
    add(`${p.id}/${c}/ulp`, lockJob(p, set, 0, nextUp(f0)));
  }
  const rs = run(jobs, opt), R = key => rs[at[key]];
  const unknown = rs.filter(r => r.summary.unknownKeys);
  row(!unknown.length, 'KEYS', `the engine knew every key on ${rs.length - unknown.length} of ${rs.length} jobs${unknown.length ? `; first refused on: ${unknown[0].name}` : ''}`);

  // ---- the jumps
  console.log(`pinned over ${PAD}: ${JSON.stringify(kPadPinned)}; ${kJumpSeconds} s at ${SR} Hz, seed ${SEED}, left pre-limiter bus`);
  const J = key => isolatedJumps(R(key).preL);
  const b460 = [];   // the as-shipped renders whose flag-on line reads a jump
  for (const os of kOsList) {
    // judged: Position spread 0 (header)
    for (const n of NOTES) {
      const on = J(`${os}/${n.name}/bs0/1`), off = J(`${os}/${n.name}/bs0/0`);
      row(on.length === 0, 'JUMP', `os ${os} ${n.name}, Position spread 0, frameHold 1: isolated jumps ${describe(on)}; required 0`);
      const must = kControlMustFire.includes(n.name);
      row(!must || off.length > 0, 'CONTROL', `os ${os} ${n.name}, Position spread 0, frameHold 0 (the present law): isolated jumps ${describe(off)}; ${must ? 'required at least 1' : 'not required'}`);
    }
    // not judged: the pad as shipped, which is B460's (header)
    for (const n of NOTES) {
      const on = J(`${os}/${n.name}/pad/1`), off = J(`${os}/${n.name}/pad/0`);
      if (on.length) b460.push([os, n]);
      note('B460', `os ${os} ${n.name}, the pad as shipped (NOT judged): frameHold 1 reads ${describe(on)}, frameHold 0 reads ${describe(off)}. ` +
        `This row is ROADMAP B460's (the spread law's steps): B460's acceptance promotes the flag-on reading to a judged row requiring 0 / 0 / 0; it is printed so the reading is on the record every run`);
    }
  }
  if (b460.length) {
    // which steps are the frame's (header, ATTRIB): printed, never judged
    const ar = run(b460.map(([os, n]) => padJob(n, { os, frameHold: 1, frame: 0 })), opt);
    b460.forEach(([os, n], i) => note('ATTRIB', `os ${os} ${n.name} (not judged): with the frame off altogether (frame 0) the pad as shipped reads ${describe(isolatedJumps(ar[i].preL))}; ` +
      `with Position spread 0 it reads ${describe(J(`${os}/${n.name}/bs0/1`))} with frameHold 1 and ${describe(J(`${os}/${n.name}/bs0/0`))} with frameHold 0`));
  }

  // ---- liveness: every flag-on render audible, and as loud as its flag-off twin
  const live = [];
  for (const os of kOsList) for (const n of NOTES) for (const sp of ['pad', 'bs0']) live.push([`pad os ${os} ${n.name}${sp === 'bs0' ? ' bspread 0' : ''}`, `${os}/${n.name}/${sp}`, false]);
  for (const p of patches) for (const [c] of CASES) live.push([`${p.id} ${c}`, `${p.id}/${c}`, p.expect === 'silent']);
  let liveBad = 0, liveWorst = 0, liveWorstName = '', liveN = 0, nSilent = 0, firstBad = '';
  for (const [name, key, wantSilent] of live) {
    const a = level(R(key + '/1')), b = level(R(key + '/0'));
    if (wantSilent) {   // the set's must-read-zero patch: silent it must be
      if (a.peak < kSilentPeak && b.peak < kSilentPeak) nSilent++;
      else { liveBad++; firstBad = firstBad || `${name}: declared silent, peaks ${a.peak.toExponential(2)} and ${b.peak.toExponential(2)}`; }
      continue;
    }
    liveN++;
    const db = 20 * Math.log10(a.rms / b.rms);
    if (!(a.peak >= kSilentPeak && b.peak >= kSilentPeak && Math.abs(db) <= kLiveDb)) { liveBad++; firstBad = firstBad || `${name}: rms ${a.rms.toExponential(2)} against ${b.rms.toExponential(2)} (${db.toFixed(2)} dB)`; }
    if (Math.abs(db) > liveWorst) { liveWorst = Math.abs(db); liveWorstName = name; }
  }
  row(liveBad === 0, 'LIVENESS', `${liveN - liveBad} of ${liveN} flag-on renders are audible and within ${kLiveDb} dB of their flag-off twin in rms (largest ${liveWorst.toFixed(2)} dB, ${liveWorstName}); ` +
    `${nSilent} render pair(s) of the declared-silent patch silent${firstBad ? `; first failure: ${firstBad}` : ''}`);

  // ---- the locked cases
  for (const [c, set] of CASES) {
    const ceil = c === 'unison' ? kUnisonCeil : kCoupledCeil, from = c === 'coupled' ? kCoupledFrom * SR : 0;
    console.log(`locked, ${c}: ${JSON.stringify(set)}; one held C3, ${kLockSeconds} s, judged from ${from / SR} s on; ceiling ${ceil}`);
    let judged = 0, worst = 0, worstId = '';
    for (const p of patches) {
      const on = R(`${p.id}/${c}/1`), off = R(`${p.id}/${c}/0`), ulp = R(`${p.id}/${c}/ulp`);
      const d = maxDiff(on, off, from), self = maxDiff(off, ulp, from);
      const early = from ? `; before ${from / SR} s (not judged) ${maxDiff(on, off, 0, from).toExponential(2)}` : '';
      // a silent render differs from nothing: it is never counted (LIVENESS judges that
      // the one patch declared silent is, and that no other is)
      if (level(on).peak < kSilentPeak && level(off).peak < kSilentPeak) { note('LOCKED', `${c} ${p.id}: silent; not counted`); continue; }
      if (!(self <= ceil)) {
        const text = `${c} ${p.id}: max |on - off| ${d.toExponential(2)}${early}; NOT JUDGED, the patch alone turns one ULP of its note into ${self.toExponential(2)}`;
        if (kMaySkip.includes(p.id)) console.log(`SKIP  LOCKED   ${text} (one of the ${kMaySkip.length} patches that may be excused)`);
        else row(false, 'LOCKED', `${text}, and it is not one of the ${kMaySkip.length} patches that may be excused (${kMaySkip.join(', ')})`);
        continue;
      }
      judged++;
      if (d > worst) { worst = d; worstId = p.id; }
      row(d <= ceil, 'LOCKED', `${c} ${p.id}: max |on - off| ${d.toExponential(2)} (its own one-ULP divergence ${self.toExponential(2)})${early}`);
    }
    row(judged >= kLockMinJudged, 'LOCKED', `${c}: ${judged} of ${patches.length} reference patches judged (need at least ${kLockMinJudged}); worst ${worst.toExponential(2)}${worstId ? ` (${worstId})` : ''}, ceiling ${ceil}`);
  }

  console.log(`h2_frame_hold_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; the jumps at os ${kOsList.join(' and ')}, their controls, liveness, and the locked cases over ${patches.length} reference patches)`);
  return red ? 1 : 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  try { process.exitCode = main(process.argv.slice(2)); }
  catch (e) {
    console.error(String(e && e.stack || e));
    console.log(`h2_frame_hold_check: RED (the run broke: ${String(e && e.message || e).split('\n')[0]})`);
    process.exitCode = 2;
  }
}
