/*
 * h2_cut_latch_check.mjs — ADR-216 D9 (`cutLatch`, ROADMAP B487) does what it was built
 * to do, on the C++ engine: with the flag on, a quantized cut reaches each blade only at
 * that blade's next cycle start, so the drift walk's flips between a cut rule's ratios
 * are no longer jumps in the wave; with it off (the present law) they are.
 *
 * WIRED: ./verify full
 *
 * THE FAULT (B485's measurement, B487). Under a cut rule (or a Cut spread) with Quantize
 * on (`kq` 1), the spread law's walk moves each member's slot, and on Glass horde pad
 * (`kRule` 4, `law` 4) the slot flips between the rule's ratios about 99 times a second
 * per member. The engine hands the law's new cut to the blades at the couple tick, at
 * once, and a blade's carrier phase is the cut times the member's phase into the window,
 * in closed form (engine.h voicePlainK: hp = kk·er), so each flip is a jump in the wave:
 * 60 % of that patch's crackle at C3 (B485). D8 SETS a quantized cut rather than ramping
 * it (ADR-216 A4 item 2), so D8 does not touch it. D9 holds the law's new cut as pending
 * and applies it at the blade's next window entry, where its carrier phase restarts
 * (engine.h latchCut, latchStep).
 *
 * WHAT IS RENDERED. Everything goes through the engine harness (h2/harness/), so this is
 * the C++ engine, never the JS one. Two builds of the harness: the shipped one
 * (h2_harness_render, --bin) for every row on the sound, and the FAULT BUILD
 * (h2_harness_render_faults, --fault-bin; the same tool with the engine's must-fail hooks
 * compiled in) for the plants and the probe (h2/harness/control_latch.h). The COMPOSE row
 * shows on every run that the fault build at fault 0 renders what the shipped one
 * renders, bit for bit. 48 kHz; the patch is `ledger/glass-horde-pad` as shipped, with
 * D8 on and the preview's settings (`frameHold` 1, `stealFade` 1; `poly` is the harness
 * cell's 8), and `os` and `cutLatch` pinned on every job.
 *
 *   WALK rows (judged; lesson 1 of D8: judge on a walk the flag cannot change). The latch
 *   draws nothing from the host stream, and on a patch with no noise blade nothing else
 *   does but the law, so the flag cannot change the spread's walk. Shown, not assumed:
 *   control/latch-probe plays one job through two engines, one with the latch and one
 *   without, and after every sample compares every sounding member's fourteen spread
 *   slots and its phase. Required: exactly 0 on every job probed here, with members
 *   compared; and its CONTROL, the same probe on a job whose blade 1 is a NOISE blade
 *   (its draws depend on the cut it is fed), must read MORE than 0.
 *
 *   JUMP rows (judged; the acceptance, read as a SHARE: lesson 2 of D8). ROADMAP B487:
 *   "the frame-hold detector reads 0 hits at a tick boundary on Glass horde pad with D8
 *   and D9 on, with its control and a plant that applies the flip at once". The pad at
 *   C2 (velocity 0.9), C3 (0.9), C4 (0.8) and a four-note chord (C3 E3 G3 C4, 0.85),
 *   seeds 1 to 8, os 2 and os 1, 8 s each. With cutLatch 1 the RAW DETECTOR HITS at a
 *   tick boundary are at most kJumpShare of those with cutLatch 0, at each os, on at
 *   least kJumpMinOff of the latter (CONTROL rows). Not "none": at C2 the detector fires
 *   on the crest of the wave's own steepest slope (D8 round 3), and such a hit falls in
 *   the boundary set by chance. Every flag-on hit is printed. The raw hits are
 *   tools/h2_spread_ramp_check.mjs's rawHits (the frame-hold check's detector before its
 *   merge, held to the original by that check's DETECTOR row), imported, not copied.
 *   AT A TICK BOUNDARY: positions 0 and 1 at os 1, 0 to 6 at os 2, D8's set and for D8's
 *   reason: with the flag off the cut is written by spread() at the couple tick, the very
 *   place D8's values are, and reaches the output along the same path.
 *   PLANT row (judged): control/latch-f15 (the latch ignored: the pending cut taken on
 *   every step, so at the first step after the tick) on the same jobs must read OVER
 *   kJumpShare, or the JUMP row could not see a latch that does nothing.
 *
 *   PROBE rows (judged; lesson 3 of D8: read what the blade is FED, not the fields).
 *   control/latch-probe taps the cut each blade receives on every member step (engine.h
 *   tapCut, the fault build only: the argument stepBlade is called with, and bx.k) and
 *   where that step's phase stands in each blade's window. A change of the fed cut is ON
 *   A CYCLE START when the step's place in that blade's window is under one step's
 *   increment (e1 < dphi): the carrier phase there is kk·e1, restarted. That is the blade
 *   law's own entry test in exact arithmetic, computed by another path; what the row
 *   asks is the property itself, that the cut a blade is fed never changes while its
 *   carrier is mid-cycle. Jobs: the pad with its cut in cycles (`lock` 0 and `lock2` 0;
 *   in Hz the cut a blade is fed moves at every tick with the member's frequency, which
 *   is not the latch's) and blade 2 given the same cut rule (`kRule2` 4), so both
 *   blades' cuts flip; blade 1 in each mode (0 sync, 1 FM reset, 2 FM free, 3 noise,
 *   4 fold, 5 ring, 6 crush); C3, seed 1, 4 s, os 1 and os 2. Required, per blade: with
 *   cutLatch 1 no change off a cycle start, and at least one on one; the same probe's
 *   flag-off engine must read changes off a cycle start (its control).
 *   LIVE rows (judged; the cut still follows the walk). At every cycle start of the
 *   flag-on engine, the cut it is fed must be EXACTLY the cut the flag-off engine is fed
 *   on the same step (the law's latest), on every non-noise probe job. Printed beside it:
 *   how many flips each fed (latest wins, so the latched engine changes less often), and
 *   the longest the two disagree, in ms, the latch's measured worst delay.
 *   PLANT rows (judged; lesson 4 of D8: every way the latch can be wrong turns a row red,
 *   on every run). The probe on the engine at each fault (control/latch-probe-f<n>,
 *   blade 1 in FM reset, both os): f15 the latch ignored, f16 one step late, f17 one step
 *   early, f18 on blade 1 only, f19 on blade 2 only, f20 never released, f21 fed stale
 *   (the fields latch, the blades are fed the law's latest). Each is required to fail
 *   the row named for it.
 *
 *   DIFF rows (judged; what the blade is fed, read on the sound). On the pad (C3, seed
 *   1, os 1 and os 2): the shipped engine with cutLatch 1 must NOT render the samples it
 *   renders with cutLatch 0 (the latch changes what the blades are fed), and
 *   control/latch-f21 (fed stale) with cutLatch 1 MUST render exactly the cutLatch 0
 *   samples, so a fed-stale engine would turn the first row red.
 *
 *   TICKABS (printed): B485's absolute tick measure, the square root of the second
 *   difference's mean square at the worst of the tick's first four positions less its
 *   median position, on the pad at C3, seed 1, both os, flag off and on.
 *
 *   D8OFF (printed, NOT judged; the lead's addition, 2026-10-14). The human's blind A/B
 *   preferred the spread ramp OFF (B460's note), so the pad may ship with D8 off. The
 *   same renders with `spreadRamp` 0, cutLatch 0 against 1: hits at a tick boundary and
 *   in all, by note, and tickAbs at C3. With D8 off blade 2's Position steps at the tick
 *   remain (D8's to remove), so a flag-on count is not expected to reach 0 here.
 *
 *   LIVENESS row (judged). Every cutLatch 1 render of the JUMP rows is audible and within
 *   kLiveDb of its cutLatch 0 twin in rms.
 *
 * ALSO REQUIRED OF EVERY JOB: the engine knew every key (a misspelt `cutLatch` would
 * render the present law twice). THE BOUNDS, ALL PROPOSED (the human's to ratify): the
 * jump share kJumpShare on at least kJumpMinOff, and D8's boundary set. Exact (no bound):
 * the walk, the probe's off-start count, the cut at a cycle start, the compose and diff
 * rows.
 *
 * THE LAST LINE IS THE VERDICT: `h2_cut_latch_check: GREEN (...)` or `RED (...)`.
 * Exit 0 green, 1 red, 2 the run broke.
 *
 *   node tools/h2_cut_latch_check.mjs [--bin <h2_harness_render>] [--fault-bin <h2_harness_render_faults>]
 *
 * Deterministic: no clock, no randomness but the engine's seeded streams. One render
 * process per seed and section, so that no more than a section's samples are held.
 */
import { realpathSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { DEFAULT_BIN, jobFor, patch, run } from '../h2/harness/harness.mjs';
import { rawHits } from './h2_spread_ramp_check.mjs';

const SR = 48000;
// The host random streams (eight seeds, as D8's check). CHANGING A SEED IS NEVER A FIX.
const kSeeds = [1, 2, 3, 4, 5, 6, 7, 8];
const SEED = 1;
const PAD = 'ledger/glass-horde-pad';
const C2 = { name: 'C2', notes: [36], vel: 0.9 }, C3 = { name: 'C3', notes: [48], vel: 0.9 }, C4 = { name: 'C4', notes: [60], vel: 0.8 };
const CHORD = { name: 'chord', notes: [48, 52, 55, 60], vel: 0.85 };
const kNotes = [C2, C3, C4, CHORD];
const kSeconds = 8, kProbeSeconds = 4;
const kOsList = [2, 1];
// what every job pins beside os and cutLatch: D8 on, and the preview's frame hold and steal fade
const kPinned = { spreadRamp: 1, frameHold: 1, stealFade: 1 };
// the engine's couple tick, in samples (h2/engine/engine.h kCoupleSamples), and D8's boundary set
const kTick = 32;
const kBoundaryLast = { 1: 1, 2: 6 };
const atBoundary = (os, at) => at % kTick <= kBoundaryLast[os];
// PROPOSED: with the latch, the raw hits at a tick boundary are at most this share of those
// without it, on at least kJumpMinOff of the latter (D8's numbers, ADR-216 A4 item 12).
const kJumpShare = 0.05, kJumpMinOff = 100;
// the probe's jobs: the cut in cycles on both blades, blade 2 under the same rule (header)
const kProbeJob = { lock: 0, lock2: 0, kRule2: 4, kRuleAmt2: 1 };
const kModes = [[0, 'sync'], [1, 'FM reset'], [2, 'FM free'], [3, 'noise'], [4, 'fold'], [5, 'ring'], [6, 'crush']];
const kNoise = 3;
const kPlantMode = 1;   // the pad's own mode
// the probe's slots (control_latch.h)
const kSlots = 64, kRead = 16;
// D9's plants: the fault, what it does, and the row it must fail
const kPlants = [
  [15, 'the latch ignored (the pending cut taken on every step)', 'PROBE', 0],
  [16, 'the cut taken one step late', 'PROBE', 0],
  [17, 'the cut taken one step early', 'PROBE', 0],
  [18, 'the latch on blade 1 only', 'PROBE', 1],
  [19, 'the latch on blade 2 only', 'PROBE', 0],
  [20, 'the latch never released', 'LIVE', 0],
  [21, 'the fields latched, the blades fed the law\'s latest', 'PROBE', 0],
];
// liveness: flag on within this many dB of flag off, and neither silent (D8's bound)
const kLiveDb = 3;
const kSilentPeak = Math.pow(10, -90 / 20);

/* B485's absolute tick measure (header, TICKABS) */
export function tickAbs(x) {
  const E = new Float64Array(kTick), C = new Float64Array(kTick);
  for (let n = 1; n < x.length - 1; n++) { const d = x[n + 1] - 2 * x[n] + x[n - 1]; E[n % kTick] += d * d; C[n % kTick]++; }
  for (let p = 0; p < kTick; p++) E[p] /= C[p];
  const s = Float64Array.from(E).sort(), med = 0.5 * (s[kTick / 2 - 1] + s[kTick / 2]);
  let first = 0;
  for (let p = 0; p < 4; p++) if (!(E[p] <= first)) first = E[p];
  return Math.sqrt(Math.max(0, first - med));
}
function level(r) {
  let e = 0, pk = 0;
  for (let i = 0; i < r.L.length; i++) { e += r.L[i] * r.L[i] + r.R[i] * r.R[i]; pk = Math.max(pk, Math.abs(r.L[i]), Math.abs(r.R[i])); }
  return { rms: Math.sqrt(e / (2 * r.L.length)), peak: pk };
}
/* the probe's readings: slot -> value, from the last kSlots samples */
function probeRead(r) {
  const s = new Array(kRead).fill(NaN);
  for (let i = r.L.length - kSlots; i < r.L.length; i++) if (r.R[i] < kRead) s[r.R[i]] = r.L[i];
  return {
    start: [s[0], s[1]], other: [s[2], s[3]], off: [s[4], s[5]], offOther: [s[6], s[7]],
    mismatch: [s[8], s[9]], starts: [s[10], s[11]], run: [s[12], s[13]], walk: s[14], members: s[15],
  };
}
const job = (seed, n, os, over, engine, seconds = kSeconds, pre = true) => jobFor({ patch: PAD, engine, sr: SR, seed, seconds, notes: n.notes, vel: n.vel, pre,
  overrides: { os, ...kPinned, ...over } });

export function main(argv) {
  const at = k => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : undefined; };
  const bin = at('--bin') || process.env.H2_HARNESS_BIN || DEFAULT_BIN;
  const faultBin = at('--fault-bin') || join(dirname(bin), 'h2_harness_render_faults');
  let red = 0, jobsN = 0, unknownN = 0, firstUnknown = '';
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(8)} ${text}`); if (!ok) red++; };
  const note = (tag, text) => console.log(`NOTE  ${tag.padEnd(8)} ${text}`);
  patch(PAD);   // throws if the pad has left the reference set
  const batch = (entries, b) => {
    const rs = run(entries.map(e => e[1]), { bin: b }), out = {};
    entries.forEach((e, i) => { out[e[0]] = rs[i]; });
    jobsN += rs.length;
    for (const r of rs) if (r.summary.unknownKeys) { unknownN++; firstUnknown = firstUnknown || r.name; }
    return out;
  };

  // ---- the acceptance renders, a seed at a time; only the readings are kept
  const hits = {};      // `${os}/${cl}` or `${os}/plant` -> [{ seed, note, hits }]
  const live = [];
  const ticks = {};
  for (const seed of kSeeds) {
    const sj = [], fj = [];
    for (const os of kOsList) for (const n of kNotes) {
      for (const cl of [1, 0]) sj.push([`${os}/${n.name}/${cl}`, job(seed, n, os, { cutLatch: cl })]);
      fj.push([`${os}/${n.name}/plant`, job(seed, n, os, { cutLatch: 1 }, 'control/latch-f15')]);
      // printed, not judged: the same pad with D8 OFF (the human's preference, B460's A/B note)
      for (const cl of [1, 0]) sj.push([`${os}/${n.name}/d8off/${cl}`, job(seed, n, os, { spreadRamp: 0, cutLatch: cl })]);
    }
    const S = batch(sj, bin), F = batch(fj, faultBin);
    for (const os of kOsList) for (const n of kNotes) {
      for (const cl of [1, 0]) {
        const r = S[`${os}/${n.name}/${cl}`], h = rawHits(r.preL);
        if (cl === 1) for (const e of h) e.near = [-2, -1, 0, 1, 2].map(k => Math.abs(r.preL[e.at + k] - r.preL[e.at + k - 1]));
        (hits[`${os}/${cl}`] ||= []).push({ seed, note: n.name, hits: h });
        if (seed === SEED && n === C3) ticks[`${os}/${cl}`] = tickAbs(r.preL);
        const r8 = S[`${os}/${n.name}/d8off/${cl}`];
        (hits[`${os}/d8off/${cl}`] ||= []).push({ seed, note: n.name, hits: rawHits(r8.preL) });
        if (seed === SEED && n === C3) ticks[`${os}/d8off/${cl}`] = tickAbs(r8.preL);
      }
      (hits[`${os}/plant`] ||= []).push({ seed, note: n.name, hits: rawHits(F[`${os}/${n.name}/plant`].preL) });
      live.push([`seed ${seed} os ${os} ${n.name}`, level(S[`${os}/${n.name}/1`]), level(S[`${os}/${n.name}/0`])]);
    }
  }

  // ---- the fault build against the shipped one, and the fed-stale differential (C3, seed 1)
  const cj = [], cf = [];
  for (const os of kOsList) for (const cl of [0, 1]) {
    cj.push([`${os}/${cl}`, job(SEED, C3, os, { cutLatch: cl }, undefined, kProbeSeconds, false)]);
    cf.push([`${os}/${cl}`, job(SEED, C3, os, { cutLatch: cl }, 'control/latch-f0', kProbeSeconds, false)]);
    if (cl === 1) cf.push([`${os}/stale`, job(SEED, C3, os, { cutLatch: 1 }, 'control/latch-f21', kProbeSeconds, false)]);
  }
  const CS = batch(cj, bin), CF = batch(cf, faultBin);

  // ---- the probe: every mode, both os; the plants; the walk on the pad as shipped
  const pj = [];
  for (const os of kOsList) {
    for (const [mode] of kModes) pj.push([`mode/${os}/${mode}`, job(SEED, C3, os, { ...kProbeJob, mode, cutLatch: 1 }, 'control/latch-probe', kProbeSeconds, false)]);
    for (const [f] of kPlants) pj.push([`plant/${os}/${f}`, job(SEED, C3, os, { ...kProbeJob, mode: kPlantMode, cutLatch: 1 }, `control/latch-probe-f${f}`, kProbeSeconds, false)]);
  }
  for (const seed of kSeeds) pj.push([`walk/${seed}`, job(seed, C3, 1, { cutLatch: 1 }, 'control/latch-probe', kProbeSeconds, false)]);
  pj.push(['walk/chord', job(SEED, CHORD, 2, { cutLatch: 1 }, 'control/latch-probe', kProbeSeconds, false)]);
  const P = {};
  for (const [k, r] of Object.entries(batch(pj, faultBin))) P[k] = probeRead(r);

  row(!unknownN, 'KEYS', `the engine knew every key on ${jobsN - unknownN} of ${jobsN} jobs${unknownN ? `; first refused on: ${firstUnknown}` : ''}`);
  console.log(`${PAD} as shipped; pinned on every job beside os and cutLatch: ${JSON.stringify(kPinned)}; ${kSeconds} s at ${SR} Hz, seeds ${kSeeds[0]} to ${kSeeds[kSeeds.length - 1]}, ` +
    `notes ${kNotes.map(n => n.name).join(' ')}, left pre-limiter bus; at a tick boundary: tick positions 0 to ${kBoundaryLast[1]} at os 1 and 0 to ${kBoundaryLast[2]} at os 2 (header)`);

  // ---- the fault build is the shipped engine at fault 0
  for (const os of kOsList) for (const cl of [0, 1]) {
    const a = CS[`${os}/${cl}`].summary.digest, b = CF[`${os}/${cl}`].summary.digest;
    row(a === b, 'COMPOSE', `os ${os}, cutLatch ${cl}, C3: the fault build at fault 0 (control/latch-f0) renders digest ${b}, the shipped harness ${a}; required the same, or the plants and the probe are not of the shipped engine`);
  }

  // ---- the walk is the same with the flag on and off
  {
    const walks = Object.entries(P).filter(([k]) => k.startsWith('walk/') || (k.startsWith('mode/') && !k.endsWith(`/${kNoise}`)));
    const bad = walks.filter(([, p]) => !(p.walk === 0 && p.members > 0));
    row(bad.length === 0 && walks.length === kSeeds.length + 1 + kOsList.length * (kModes.length - 1), 'WALK',
      `control/latch-probe on ${walks.length} jobs (the pad as shipped at C3, every seed, os 1; its chord at os 2; and the probe's non-noise jobs): the spread slots and phases of an engine with the latch and one without differ by exactly 0 on ` +
      `${walks.length - bad.length}, with ${Math.min(...walks.map(([, p]) => p.members))} member-samples compared at the least; required all${bad.length ? `; first to differ: ${bad[0][0]}, by ${bad[0][1].walk}` : ''}`);
    const ctl = kOsList.map(os => P[`mode/${os}/${kNoise}`].walk);
    row(ctl.every(w => w > 0), 'WALK', `its control, blade 1 a noise blade (whose draws follow the cut it is fed), os ${kOsList.join(' and ')}: the two walks differ by ${ctl.map(w => w.toPrecision(3)).join(' and ')}; required more than 0 at each`);
  }

  // ---- the acceptance: raw hits at a tick boundary, as a share of the control
  const flat = key => (hits[key] || []).flatMap(c => c.hits.map(e => ({ ...e, seed: c.seed, note: c.note })));
  const one = e => `${e.step.toFixed(3)} at ${e.at} (seed ${e.seed}, ${e.note}, tick position ${e.at % kTick})`;
  const rendersWant = kSeeds.length * kNotes.length;
  for (const os of kOsList) {
    const on = flat(`${os}/1`), off = flat(`${os}/0`), pl = flat(`${os}/plant`);
    const onB = on.filter(e => atBoundary(os, e.at)), offB = off.filter(e => atBoundary(os, e.at)), plB = pl.filter(e => atBoundary(os, e.at));
    const n = (hits[`${os}/1`] || []).length;
    const share = c => `${(100 * c / offB.length).toFixed(1)} %`;
    row(offB.length >= kJumpMinOff, 'CONTROL', `os ${os}, the pad, cutLatch 0 (the present law), ${n} renders: raw detector hits at a tick boundary ${offB.length}, of ${off.length} in all; required at least ${kJumpMinOff}, or a share of them would mean nothing`);
    row(onB.length <= kJumpShare * offB.length && offB.length >= kJumpMinOff && n === rendersWant, 'JUMP',
      `os ${os}, the pad, cutLatch 1, ${n} renders: raw detector hits at a tick boundary ${onB.length}, which is ${share(onB.length)} of the ${offB.length} with cutLatch 0; required at most ${100 * kJumpShare} % (PROPOSED). ` +
      `${on.length} raw hit(s) in all with cutLatch 1, against ${off.length} with cutLatch 0`);
    for (const e of on) note('JUMP', `os ${os}, a cutLatch 1 hit: ${one(e)}, ${atBoundary(os, e.at) ? 'AT a tick boundary (counted)' : 'not at a tick boundary (not counted)'}; the steps from two samples before it to two after: ${e.near.map(v => v.toFixed(3)).join(' ')}`);
    for (const nn of kNotes) {
      const c = k => flat(k).filter(e => e.note === nn.name);
      note('JUMP', `os ${os} ${nn.name} (by note): raw hits at a tick boundary / in all: cutLatch 1 ${c(`${os}/1`).filter(e => atBoundary(os, e.at)).length} / ${c(`${os}/1`).length}; ` +
        `cutLatch 0 ${c(`${os}/0`).filter(e => atBoundary(os, e.at)).length} / ${c(`${os}/0`).length}; control/latch-f15 ${c(`${os}/plant`).filter(e => atBoundary(os, e.at)).length} / ${c(`${os}/plant`).length}`);
    }
    row(plB.length > kJumpShare * offB.length && offB.length >= kJumpMinOff, 'PLANT', `os ${os}, control/latch-f15 (the latch ignored) on the same ${(hits[`${os}/plant`] || []).length} jobs, read as the JUMP row reads the engine: ` +
      `raw detector hits at a tick boundary ${plB.length}, which is ${share(plB.length)} of the ${offB.length} with cutLatch 0 (${pl.length} in all); required OVER ${100 * kJumpShare} %, or the JUMP row could not see a latch that does nothing`);
  }
  for (const os of kOsList) note('TICKABS', `os ${os} C3 seed ${SEED} (B485's measure, not judged): cutLatch 0 ${ticks[`${os}/0`].toExponential(3)}, cutLatch 1 ${ticks[`${os}/1`].toExponential(3)}`);
  // printed, NOT judged: the same measure with D8 OFF (header, D8OFF)
  for (const os of kOsList) {
    const on = flat(`${os}/d8off/1`), off = flat(`${os}/d8off/0`);
    note('D8OFF', `os ${os}, the pad as shipped with spreadRamp 0, ${(hits[`${os}/d8off/1`] || []).length} renders (printed, not judged): raw detector hits at a tick boundary / in all: ` +
      `cutLatch 0 ${off.filter(e => atBoundary(os, e.at)).length} / ${off.length}, cutLatch 1 ${on.filter(e => atBoundary(os, e.at)).length} / ${on.length}; ` +
      `tickAbs at C3 seed ${SEED}: cutLatch 0 ${ticks[`${os}/d8off/0`].toExponential(3)}, cutLatch 1 ${ticks[`${os}/d8off/1`].toExponential(3)}`);
    for (const nn of kNotes) {
      const c = k => flat(k).filter(e => e.note === nn.name);
      note('D8OFF', `os ${os} ${nn.name} (by note): at a tick boundary / in all: cutLatch 0 ${c(`${os}/d8off/0`).filter(e => atBoundary(os, e.at)).length} / ${c(`${os}/d8off/0`).length}; ` +
        `cutLatch 1 ${c(`${os}/d8off/1`).filter(e => atBoundary(os, e.at)).length} / ${c(`${os}/d8off/1`).length}`);
    }
  }

  // ---- what the blades are fed: the probe
  console.log(`what each blade is FED (control/latch-probe, the fault build's tap): the pad with ${JSON.stringify(kProbeJob)}, C3, seed ${SEED}, ${kProbeSeconds} s; a change is on a cycle start when the step's place in that blade's window is under one step's increment`);
  for (const os of kOsList) for (const [mode, name] of kModes) {
    const p = P[`mode/${os}/${mode}`];
    for (const b of [0, 1]) {
      row(p.other[b] === 0 && p.start[b] > 0 && p.offOther[b] > 0, 'PROBE',
        `os ${os}, blade 1 ${name}, blade ${b + 1}: the cut it is fed changed ${p.start[b]} time(s) on a cycle start and ${p.other[b]} off one with cutLatch 1; required 0 off one, and at least 1 on one; ` +
        `its control, the same job with cutLatch 0: ${p.offOther[b]} of its ${p.off[b]} changes off a cycle start; required at least 1`);
    }
    if (mode === kNoise) { note('LIVE', `os ${os}, blade 1 noise (not judged: the two engines walk apart, WALK's control): at a cycle start the latched engine was fed another cut than the present law on ${p.mismatch[0]} / ${p.mismatch[1]} of ${p.starts[0]} / ${p.starts[1]}`); continue; }
    const ms = s => `${(1000 * s / (SR * os)).toFixed(2)} ms`;
    row(p.mismatch.every(x => x === 0) && p.starts.every(x => x > 0), 'LIVE',
      `os ${os}, blade 1 ${name}: at every cycle start the latched engine is fed the cut the present law is fed on that step: mismatches ${p.mismatch[0]} / ${p.mismatch[1]} of ${p.starts[0]} / ${p.starts[1]} cycle starts (blade 1 / blade 2); required 0 of more than 0. ` +
      `Flips fed: ${p.start[0]} / ${p.start[1]} latched against ${p.off[0]} / ${p.off[1]} at once; the longest the two disagree: ${ms(p.run[0])} / ${ms(p.run[1])}`);
  }
  // the plants: each must fail the row named for it
  for (const os of kOsList) for (const [f, what, rowName, b] of kPlants) {
    const p = P[`plant/${os}/${f}`];
    const probeRed = !(p.other[b] === 0 && p.start[b] > 0), liveRed = !(p.mismatch.every(x => x === 0) && p.starts.every(x => x > 0));
    const caught = rowName === 'PROBE' ? probeRed : liveRed;
    row(caught, 'PLANT', `os ${os}, control/latch-probe-f${f} (${what}): blade ${b + 1} fed changes on / off a cycle start ${p.start[b]} / ${p.other[b]}; cycle-start mismatches ${p.mismatch[0]} / ${p.mismatch[1]}; ` +
      `required to FAIL the ${rowName} row${rowName === 'PROBE' ? ` on blade ${b + 1}` : ''}, or this check cannot see that fault`);
  }

  // ---- the differential on the sound: the latch changes what is fed; a fed-stale engine does not
  for (const os of kOsList) {
    const on = CS[`${os}/1`].summary.digest, off = CS[`${os}/0`].summary.digest, stale = CF[`${os}/stale`].summary.digest;
    row(on !== off, 'DIFF', `os ${os}, C3: cutLatch 1 renders digest ${on}, cutLatch 0 ${off}; required different (the latch changes what the blades are fed)`);
    row(stale === off, 'DIFF', `os ${os}, its plant: control/latch-f21 (fields latched, the blades fed the law's latest) with cutLatch 1 renders ${stale}; required exactly the cutLatch 0 samples, so the row above would be red on it`);
  }

  // ---- liveness: every cutLatch 1 render audible, and as loud as its cutLatch 0 twin
  let liveBad = 0, liveWorst = 0, liveWorstName = '', firstBad = '';
  for (const [name, a, b] of live) {
    const db = 20 * Math.log10(a.rms / b.rms);
    if (!(a.peak >= kSilentPeak && b.peak >= kSilentPeak && Math.abs(db) <= kLiveDb)) { liveBad++; firstBad = firstBad || `${name}: rms ${a.rms.toExponential(2)} against ${b.rms.toExponential(2)} (${db.toFixed(2)} dB)`; }
    if (Math.abs(db) > liveWorst) { liveWorst = Math.abs(db); liveWorstName = name; }
  }
  row(liveBad === 0 && live.length === kOsList.length * rendersWant, 'LIVENESS', `${live.length - liveBad} of ${live.length} cutLatch 1 renders are audible and within ${kLiveDb} dB of their cutLatch 0 twin in rms (largest ${liveWorst.toFixed(2)} dB, ${liveWorstName})` +
    `${firstBad ? `; first failure: ${firstBad}` : ''}`);

  console.log(`h2_cut_latch_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; the pad as shipped with D8, D9 against its control and the latch-ignored plant at a tick boundary over ${kSeeds.length} seeds, ` +
    `${kNotes.length} notes and os ${kOsList.join(' and ')}; the walk; what each blade is fed in ${kModes.length} modes and ${kPlants.length} plants; the cut at every cycle start; the fault build; the fed-stale differential; liveness)`);
  return red ? 1 : 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  try { process.exitCode = main(process.argv.slice(2)); }
  catch (e) {
    console.error(String(e && e.stack || e));
    console.log(`h2_cut_latch_check: RED (the run broke: ${String(e && e.message || e).split('\n')[0]})`);
    process.exitCode = 2;
  }
}
