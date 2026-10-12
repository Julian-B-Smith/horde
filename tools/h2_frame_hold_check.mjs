/*
 * h2_frame_hold_check.mjs — ADR-216 D6 (`frameHold`, ROADMAP B461) does what it was
 * ruled to do, on the C++ engine: with the flag on, a held note of the pad the human
 * heard click reads no isolated jump; with it off (the present law) it reads them;
 * and where the swarm is locked the flag changes nothing above a stated ceiling.
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
 * WHAT IS RENDERED. Everything goes through the engine harness (h2/harness/), so
 * this is the C++ engine as it ships, never the JS one.
 *
 *   JUMP rows. One held note of `ledger/breathing-pad`, 48 kHz, 8 s, host seed 1, at
 *   C3 (velocity 0.9), G3 (0.9) and C4 (0.8), with frameHold 1. An ISOLATED JUMP is a
 *   sample-to-sample step on the left channel of the pre-limiter bus that is BOTH
 *   over 0.02 AND over 3 times the 99th percentile of the steps around it: the
 *   percentile is taken per 100 ms window (4800 samples) over the window and 2400
 *   samples either side, and events within 480 samples are one. Required: none, at
 *   all three notes.
 *
 *   CONTROL rows (LIBRARY L0032: a detector that cannot see the fault proves
 *   nothing). The same three renders with frameHold 0, the present law. Required:
 *   at least one isolated jump at C3 AND at C4, or this check is RED whatever the
 *   flag-on rows read. (The lead measured 5 and 2 there on 2026-10-12, the largest
 *   0.34 and 0.60. G3 is printed and not required.)
 *
 *   THE PAD'S SETTINGS ARE PINNED HERE, NOT LEFT TO THE BANK (the lead, 2026-10-12).
 *   The click needs the members to drift apart and re-form, so it depends on the
 *   patch: `detune` 14 and `K` 0.45 (with detune 0 the members stay locked and there
 *   is nothing to reproduce), a frame that follows the swarm (`frame` 1), settled
 *   start phases (`phaseMode` 2), six members (`N` 6) and the drifting spread law
 *   (`law` 4 at `driftRate` 0.1). Those seven are the values under which the lead
 *   measured the click. They are written over the reference patch on every JUMP and
 *   CONTROL job, so an edit of the bank's pad cannot quietly blind the control: the
 *   rest of the patch is still the bank's, and if a later edit there removes the
 *   click the CONTROL row turns this check red and says so.
 *
 *   ATTRIBUTION rows, printed only when a flag-on note reads a jump, never judged.
 *   The same detector also sees steps that are not the frame's (B460: the spread
 *   law's values reach the blades unsmoothed every 32 samples). For each such note
 *   two more renders say which it is: the pad with the frame off altogether
 *   (`frame` 0: whatever it reads, the frame did not make), and the pad with the
 *   spread law's one route to the blade's POSITION closed (`bspread` 0), flag on.
 *
 *   LOCKED rows (acceptance 3). Every reference patch in h2/harness/patches.json,
 *   one held C3, 4 s, with the members locked BY CONSTRUCTION and the frame on, so
 *   that each lead does reach a blade: kLock below (one frequency, one start phase,
 *   no coupling, no onset burst, no swarm drift, no member waiting; R is 1). The
 *   flag-on render against the flag-off render, largest |difference| over both
 *   output channels. With the swarm locked the frame IS the measured centre bit for
 *   bit (holdFrame), so the leads' targets are the present law's and only their
 *   last-bit wobble is ramped instead of stepped.
 *   The ceiling is MEASURED, not chosen to pass: 2026-10-12, darwin-arm64, the
 *   worst judged row read 6.7e-14 (`ledger/defaults`); kLockCeil is 1e-11, about
 *   150 times that, five orders under the parity bound (1e-6).
 *   A patch that cannot show a difference that small is NOT JUDGED, on evidence
 *   re-measured every run (ADR-065's rule): each patch is also rendered against
 *   ITSELF, flag off, its note one ULP sharp. If that alone moves the output by
 *   more than the ceiling, the patch amplifies a last-bit change (feedback and
 *   cross-modulation at their maxima: the two loud corner patches) and its row is
 *   printed as such. At least kLockMinJudged rows must be judged.
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

const SR = 48000, SEED = 1;
const PAD = 'ledger/breathing-pad';
// The seven settings the click depends on, as the lead measured it (header).
export const kPadPinned = { frame: 1, phaseMode: 2, N: 6, law: 4, driftRate: 0.1, detune: 14, K: 0.45 };
const NOTES = [{ name: 'C3', note: 48, vel: 0.9 }, { name: 'G3', note: 55, vel: 0.9 }, { name: 'C4', note: 60, vel: 0.8 }];
const kJumpSeconds = 8;
const kControlMustFire = ['C3', 'C4'];
// the detector (header): a step over kStepFloor AND over kRatio x the local 99th percentile
const kStepFloor = 0.02, kRatio = 3, kPercentile = 0.99;
const kWindow = Math.round(0.1 * SR), kFlank = kWindow / 2, kMerge = 480;
// the locked case (header)
export const kLock = { frame: 1, phaseMode: 2, detune: 0, K: 0, onset: 0, driftDepth: 0, onsetScatter: 0 };
const kLockNote = 48, kLockVel = 0.9, kLockSeconds = 4;
const kLockCeil = 1e-11, kLockMinJudged = 5;

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
function maxDiff(a, b) {
  let m = 0;
  for (let i = 0; i < a.L.length; i++) {
    const l = Math.abs(a.L[i] - b.L[i]), r = Math.abs(a.R[i] - b.R[i]);
    if (!(l <= m)) m = l;   // a NaN difference is kept, and fails every bound
    if (!(r <= m)) m = r;
  }
  return m;
}

const padJob = (n, over) => jobFor({ patch: PAD, sr: SR, seed: SEED, seconds: kJumpSeconds, notes: [n.note], vel: n.vel, pre: true,
  overrides: { ...kPadPinned, ...over } });
const lockJob = (p, flag, freq) => jobFor({ patch: p, sr: SR, seed: SEED,
  cmds: [...patchCmds(p, { ...kLock, frameHold: flag }), ['on', kLockNote, freq, kLockVel], ['render', kLockSeconds * SR]] });

export function main(argv) {
  const bi = argv.indexOf('--bin'), opt = bi >= 0 ? { bin: argv[bi + 1] } : {};
  let red = 0;
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(8)} ${text}`); if (!ok) red++; };
  const patches = loadPatches().patches;
  patch(PAD);   // throws if the pad has left the reference set

  // one process renders everything: 3 x (on, off), then 3 per reference patch
  const jobs = [];
  for (const n of NOTES) jobs.push(padJob(n, { frameHold: 1 }), padJob(n, { frameHold: 0 }));
  const f0 = mtof(kLockNote);
  for (const p of patches) jobs.push(lockJob(p, 1, f0), lockJob(p, 0, f0), lockJob(p, 0, nextUp(f0)));
  const rs = run(jobs, opt);
  const unknown = rs.filter(r => r.summary.unknownKeys);
  row(!unknown.length, 'KEYS', `the engine knew every key on ${rs.length - unknown.length} of ${rs.length} jobs${unknown.length ? `; first refused on: ${unknown[0].name}` : ''}`);

  console.log(`pinned over ${PAD}: ${JSON.stringify(kPadPinned)}; ${kJumpSeconds} s at ${SR} Hz, seed ${SEED}, left pre-limiter bus`);
  const redNotes = [];
  NOTES.forEach((n, i) => {
    const on = isolatedJumps(rs[2 * i].preL), off = isolatedJumps(rs[2 * i + 1].preL);
    row(on.length === 0, 'JUMP', `${n.name} frameHold 1: isolated jumps ${describe(on)}; required 0`);
    if (on.length) redNotes.push(n);
    const must = kControlMustFire.includes(n.name);
    row(!must || off.length > 0, 'CONTROL', `${n.name} frameHold 0 (the present law): isolated jumps ${describe(off)}; ${must ? 'required at least 1' : 'not required'}`);
  });
  if (redNotes.length) {
    // which steps are the frame's (header, ATTRIBUTION): printed, never judged
    const aj = [];
    for (const n of redNotes) aj.push(padJob(n, { frameHold: 1, frame: 0 }), padJob(n, { frameHold: 1, bspread: 0 }), padJob(n, { frameHold: 0, bspread: 0 }));
    const ar = run(aj, opt);
    redNotes.forEach((n, i) => console.log(`NOTE  ATTRIB   ${n.name} (not judged): the frame off altogether (frame 0) reads ${describe(isolatedJumps(ar[3 * i].preL))}; ` +
      `Position spread 0 reads ${describe(isolatedJumps(ar[3 * i + 1].preL))} with frameHold 1 and ${describe(isolatedJumps(ar[3 * i + 2].preL))} with frameHold 0`));
  }

  console.log(`locked by construction, the frame on: ${JSON.stringify(kLock)}; one held C3, ${kLockSeconds} s; ceiling ${kLockCeil}`);
  let judged = 0, worst = 0, worstId = '';
  patches.forEach((p, i) => {
    const b = 2 * NOTES.length + 3 * i, d = maxDiff(rs[b], rs[b + 1]), self = maxDiff(rs[b + 1], rs[b + 2]);
    if (self > kLockCeil || self !== self) {
      console.log(`SKIP  LOCKED   ${p.id}: max |on - off| ${d.toExponential(2)}; NOT JUDGED, the patch alone turns one ULP of its note into ${self.toExponential(2)}`);
      return;
    }
    judged++;
    if (d > worst) { worst = d; worstId = p.id; }
    row(d <= kLockCeil, 'LOCKED', `${p.id}: max |on - off| ${d.toExponential(2)} (its own one-ULP divergence ${self.toExponential(2)})`);
  });
  row(judged >= kLockMinJudged, 'LOCKED', `${judged} of ${patches.length} reference patches judged (need at least ${kLockMinJudged}); worst ${worst.toExponential(2)}${worstId ? ` (${worstId})` : ''}, ceiling ${kLockCeil}`);

  console.log(`h2_frame_hold_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; flag on, control, locked case over ${patches.length} reference patches)`);
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
