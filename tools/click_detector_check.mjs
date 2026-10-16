/*
 * click_detector_check.mjs — the oracle for tools/click_detector.mjs (ROADMAP B490; ADR-225
 * item 12: the detector is the acceptance-test author's first deliverable, its method ratified,
 * its numbers proposed here with must-fire and must-not-fire controls, and ratified by the human: ADR-227).
 *
 * WIRED: ./verify fast (--synthetic) and ./verify full (--bin, the engine half)
 *
 * TWO HALVES.
 *   --synthetic   signals built here, no engine: seconds. Run by ./verify fast.
 *   --bin <h2_harness_render>   the C++ engine through h2/harness/, as the installed preview plays
 *                 it. Run by ./verify full (which runs fast first, so the synthetic half is not
 *                 repeated). About a minute.
 *
 * THE DETECTOR'S NUMBERS are its DEFAULTS, read from the module, never written here: a ruling that
 * changes one changes it in one place, and every row below re-judges it.
 *
 * WHAT A PLANT IS. At a seeded position (mulberry32 from kSeed), on the left channel:
 *   step    every sample from the position on is raised by s (the brief's "step").
 *   spike   one sample raised by s.
 *   corner  a slope discontinuity: the rise s spread linearly over kCornerLen samples, so two
 *           corners (at the start and the end of the ramp) and no step.
 * s is a fraction of the render's own peak (0.02, 0.05, 0.1, 0.2), so the plant is level-relative
 * like the detector. A plant is CAUGHT when the planted signal reads an event within kCatch samples
 * of it (for a corner, of either corner) AND the same excerpt without the plant reads none there
 * (a patch's own event at that spot is not a catch). Each plant is judged on its own excerpt: from
 * warmup + kLead before the plant to kTail after it, which holds everything the detector's
 * decision at the plant reads (its predictors look back at most warmup samples; its local level
 * and analysis band look ahead less than kTail).
 *
 * THE SYNTHETIC HALF (fast).
 *   MUSTNOT (judged)  smooth signals read no event: a sine at C4, band-limited saws (every harmonic
 *                     below Nyquist, the harshest case for the pitch stage) at C1, C2, C4, C6 and
 *                     C7, an FM tone (carrier 220 Hz, ratio 1.41, index 3), seeded white noise
 *                     and seeded one-pole low-passed noise.
 *   MUSTFIRE (judged) a 0.05 step on the sine C4 and the saws C2 and C6: caught at every one of
 *                     kSynPlants positions. Spikes and corners printed.
 *   LIMIT (printed)   a chord of band-limited saws and three detuned saws at C3: what each scores
 *                     by itself and what a 0.05 and a 0.2 step score on it (the pitch predictor
 *                     follows one period; click_detector.mjs, WHAT IT CANNOT TELL APART).
 *   STAGE (judged)    the pitch stage is load-bearing: with it off (ltp false) the clean saw at
 *                     C2 must read events (its resets). A stage that could be removed with nothing
 *                     turning red would be a claim with no evidence. (The analysis band's and the
 *                     first difference's rows are in the engine half: the engine's signals are
 *                     where each was measured to matter.)
 *   LEVEL (judged)    the saw at C2 with three plants (0.02, 0.05, 0.2), at gains 0.01, 1 and 4:
 *                     the same event samples, scores equal within kLevelTol relative.
 *   NONFINITE (judged) a NaN in the input throws.
 *   OLD (printed)     the isolated-jump detector of tools/h2_frame_hold_check.mjs (unchanged) on the
 *                     same 0.05 step plants of the saws at C2 and C6, at peak 1.
 *
 * THE ENGINE HALF (full). Every render: 48 kHz, host seed 1, the preview's flags kPreview (os 1,
 * frameHold 1, stealFade 1, spreadRamp 1) before `snap`, poly 8 after it, velocity 0.85, held
 * kSeconds; the output (post-limiter) channels, as heard.
 *   MUSTFIRE (judged) a 0.05 step caught at all kPlants positions on: a sine at C4, a bare saw at C2
 *                     and at C6 (one member, no blade: the SAW and SINE settings below) and
 *                     Crushed bells at C4. THE EXCEPTIONS, printed and NOT judged, each with its
 *                     reason measured on every run: Glass horde pad C4, Breathing pad C3 and the
 *                     Crushed bells chord. Each reads, by itself, events at or over the score a
 *                     0.05 step earns there (its own discontinuities: the research note names
 *                     them), so no per-sample test can tell the plant from the patch. The size at
 *                     which each is caught every time is printed.
 *   B490 (judged)     B490's own wording: Crushed bells at C4, a step of 0.05 of FULL SCALE (not
 *                     of its peak), caught every time.
 *   MUSTNOT (judged)  the smooth engine set, the sine and the bare saw held at C1 to C7, both
 *                     channels: no event.
 *   STAGE (judged)    the analysis band: with it open (band 1) the bare saw at C1 must read events
 *                     (its resets' residue near Nyquist). The first difference: the weakest 0.05
 *                     step on Crushed bells C4 over the patch's own largest score must be larger
 *                     with it than without (diff false).
 *   NAIVE (printed)   the plain saw (base 2), held at C1 to C7: its events (the SAW comment).
 *   BANK (printed, NOT judged) the 83 bank presets on the chord C4 E4 G4 B4, and the defaults,
 *                     Crushed bells, Glass horde pad and Breathing pad held at C1 to C7, both
 *                     channels: the events of each render, by class. There is no ground truth for
 *                     these renders, so no row can call an event false; the classes are evidence:
 *                       tick      in a render whose events crowd onto the engine's 32-sample tick
 *                                 (one run of 6 tick phases holds at least half of at least 5
 *                                 events; chance is 6/32): steps the engine makes at its tick.
 *                       period    another event (over 6) one note period away (or half or twice
 *                                 one, within 3 %): a discontinuity of the tone that recurs with it.
 *                       isolated  neither: a false event, or a real step that does not recur.
 *                     --events <file> writes every event with its context as JSON.
 *   LEVEL (judged)    Crushed bells C4 with a 0.05 plant and the Screamer chord (which reads events
 *                     by itself), at gains 0.01, 1 and 4: the same events.
 *   OLD (printed)     the old detector on the same Crushed bells steps (single note and chord), on
 *                     the render as it is and scaled to peak 1.
 *   TABLE (printed)   per patch, the share of 0.05 steps caught at k = kSweep; and the events on
 *                     the smooth set, the bank chords and the held notes at each k.
 *
 * THE LAST LINE IS THE VERDICT: `click_detector_check: GREEN (...)` or `RED (...)`.
 * Exit 0 green, 1 red, 2 the run broke.
 *   node tools/click_detector_check.mjs --synthetic
 *   node tools/click_detector_check.mjs --bin <h2_harness_render> [--events <file.json>]
 * Deterministic: no clock; positions and noise from seeded mulberry32 streams.
 */
import { readFileSync, realpathSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { DEFAULTS, detectClicks, scores, eventsOf, warmup } from './click_detector.mjs';
import { isolatedJumps } from './h2_frame_hold_check.mjs';

const SR = 48000;
const kSeed = 0x0C11C4;
const kSizes = [0.02, 0.05, 0.1, 0.2];
const kCornerLen = 48;          // samples (1 ms at 48 kHz)
const kCatch = 8;               // samples either side of a plant
const kLead = Math.round(0.02 * SR), kTail = Math.round(0.05 * SR);
const kSynSeconds = 1.5, kSynPlants = 6;
const kSeconds = 2, kPlants = 10, kSmallPlants = 5;   // steps; spikes and corners
// The residual is a cancellation of terms up to about 1e7 times larger than itself (a saw's
// prediction), so scaling by a gain that is not a power of two moves a score by up to about 1e-7
// relative (measured up to 5.8e-7, on the Screamer chord, 2026-10-16). An event whose score sits
// that close to k could flip; none of the judged ones does.
const kLevelTol = 1e-6;
const kGains = [0.01, 1, 4];
const kSweep = [6, 8, 10, 12, 15, 20, 25, 30];
const kPreview = { os: 1, frameHold: 1, stealFade: 1, spreadRamp: 1 };
const CHORD = [60, 64, 67, 71], VEL = 0.85;
const kTick = 32, kTickPhases = 6, kTickMin = 5;   // chance puts 6/32 of events in a run
const mtof = n => 440 * Math.pow(2, (n - 69) / 12);
const K = DEFAULTS.k;

// the same stream as tools/h2_scenarios.mjs's mulberry32 (not imported: that module installs the
// lab sandbox guard when it loads)
function mulberry32(a) {
  return () => {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
const peakOf = x => { let m = 0; for (const v of x) m = Math.max(m, Math.abs(v)); return m; };

/* ---------------------------------------------------------------- plants */
function plant(x, at, kind, s) {
  const y = Float64Array.from(x);
  if (kind === 'step') for (let i = at; i < y.length; i++) y[i] += s;
  else if (kind === 'spike') y[at] += s;
  else for (let i = at; i < y.length; i++) y[i] += s * Math.min(i - at, kCornerLen) / kCornerLen;
  return y;
}
/* positions: uniform over the judged span of the whole render, at least one excerpt from either end */
function positions(n, count, seed) {
  const r = mulberry32(seed), lo = warmup(SR) + kLead + 1, hi = n - kTail - 1, out = [];
  for (let i = 0; i < count; i++) out.push(lo + Math.floor(r() * (hi - lo)));
  return out;
}
/* one plant judged on its excerpt (header): { score, cleanScore } = the largest score in the catch
   window with and without the plant; caught at k when score > k and cleanScore <= k */
function plantReading(x, at, kind, s, params) {
  const from = at - warmup(SR, params) - kLead, to = Math.min(x.length, at + kTail);
  const ex = x.subarray(from, to), a = at - from;
  const win = [a - kCatch, a + (kind === 'corner' ? kCornerLen : 0) + kCatch];
  const top = sc => { let m = 0; for (let i = win[0]; i <= win[1]; i++) m = Math.max(m, sc[i]); return m; };
  return { score: top(scores(plant(ex, a, kind, s), SR, params).score), cleanScore: top(scores(ex, SR, params).score) };
}
const caughtAt = (rd, k) => rd.filter(q => q.score > k && q.cleanScore <= k).length;
function plantSet(x, kind, s, count, seed, params) {
  const pk = peakOf(x);
  return positions(x.length, count, seed).map(at => plantReading(x, at, kind, s * pk, params));
}

/* ---------------------------------------------------------------- synthetic signals */
function sines(parts, n) {
  const x = new Float64Array(n);
  for (const [f, a, ph] of parts) { const w = 2 * Math.PI * f / SR; for (let i = 0; i < n; i++) x[i] += a * Math.sin(w * i + ph); }
  return x;
}
const blsaw = (f, n, a = 1) => { const p = []; for (let h = 1; h * f < SR / 2; h++) p.push([h * f, a * (2 / Math.PI) * (h % 2 ? 1 : -1) / h, 0]); return sines(p, n); };
/* seeded Gaussian noise (Box-Muller on mulberry32), through a one-pole low-pass of the given pole (0: white) */
function noise(n, seed, pole) {
  const r = mulberry32(seed), x = new Float64Array(n);
  let y = 0;
  for (let i = 0; i < n; i++) {
    const g = Math.sqrt(-2 * Math.log(Math.max(r(), 1e-300))) * Math.cos(2 * Math.PI * r());
    y = pole * y + g; x[i] = y;
  }
  return x;
}
function fm(n) {
  const x = new Float64Array(n), wc = 2 * Math.PI * 220 / SR, wm = wc * 1.41;
  for (let i = 0; i < n; i++) x[i] = Math.sin(wc * i + 3 * Math.sin(wm * i));
  return x;
}

/* ---------------------------------------------------------------- shared rows */
function levelRow(row, tag, name, x) {
  const ref = detectClicks(x, SR);
  let ok = ref.length > 0, worst = 0, detail = '';
  for (const g of kGains) {
    const ev = detectClicks(x.map(v => v * g), SR);
    if (ev.length !== ref.length || ev.some((e, i) => e.sample !== ref[i].sample)) { ok = false; detail = `gain ${g}: ${ev.length} events at ${ev.map(e => e.sample).join(',')} against ${ref.map(e => e.sample).join(',')}`; break; }
    for (let i = 0; i < ev.length; i++) worst = Math.max(worst, Math.abs(ev[i].score - ref[i].score) / ref[i].score);
  }
  row(ok && worst <= kLevelTol, tag, `${name}: ${ref.length} event(s) at ${ref.map(e => e.sample).join(', ')}; at gains ${kGains.join(', ')} the same samples, largest relative score difference ${worst.toExponential(2)} (tolerance ${kLevelTol})${detail ? '; ' + detail : ''}`);
}
const oldCatches = (x, ats, s) => ats.filter(at => isolatedJumps(plant(x, at, 'step', s)).some(e => Math.abs(e.at - at) <= kCatch)).length;
const fmtSc = rd => rd.map(q => q.score.toFixed(1)).join(' ');

/* ---------------------------------------------------------------- the synthetic half */
function synthetic(row, note) {
  const n = Math.round(kSynSeconds * SR);
  const sum = xs => xs.reduce((a, b) => a.map((v, i) => v + b[i]));
  const SMOOTH = [
    ['sine C4', sines([[mtof(60), 0.5, 0]], n)], ...[24, 36, 60, 84, 96].map(m => [`band-limited saw C${m / 12 - 1}`, blsaw(mtof(m), n, 0.5)]),
    ['FM tone 220 Hz ratio 1.41 index 3', fm(n)], ['white noise', noise(n, kSeed, 0)], ['low-passed noise (pole 0.95)', noise(n, kSeed + 1, 0.95)],
  ];
  const LIMIT = [
    ['band-limited saw chord C4 E4 G4 B4', sum(CHORD.map(m => blsaw(mtof(m), n, 0.2)))],
    ['three band-limited saws at C3, -8 / 0 / +8 cents', sum([-8, 0, 8].map(c => blsaw(mtof(48 + c / 100), n, 0.25)))],
  ];
  const sig = name => [...SMOOTH, ...LIMIT].find(s => s[0] === name)[1];
  const top = x => scores(x, SR).score.reduce((m, v) => Math.max(m, v), 0);
  console.log(`synthetic signals: ${kSynSeconds} s at ${SR} Hz; detector DEFAULTS ${JSON.stringify(DEFAULTS)} (RATIFIED, ADR-227; ADR-225 item 12); first judged sample ${warmup(SR)}`);
  let smoothMax = 0;
  for (const [name, x] of SMOOTH) {
    const res = scores(x, SR), ev = eventsOf(res, SR);
    const mx = res.score.reduce((m, v) => Math.max(m, v), 0); smoothMax = Math.max(smoothMax, mx);
    row(ev.length === 0, 'MUSTNOT', `${name}: ${ev.length} event(s)${ev.length ? ' at ' + ev.slice(0, 5).map(q => `${q.sample} (${q.score.toFixed(1)})`).join(', ') : ''}; largest score ${mx.toFixed(2)} (k ${K})`);
  }
  note('MUSTNOT', `largest score over the smooth synthetic set ${smoothMax.toFixed(2)} (k ${K})`);
  ['sine C4', 'band-limited saw C2', 'band-limited saw C6'].forEach((name, i) => {
    const x = sig(name), st = plantSet(x, 'step', 0.05, kSynPlants, kSeed + 10 + i);
    row(caughtAt(st, K) === kSynPlants, 'MUSTFIRE', `${name}: 0.05 x peak steps caught ${caughtAt(st, K)} of ${kSynPlants} (scores ${fmtSc(st)})`);
    const sp = plantSet(x, 'spike', 0.05, kSynPlants, kSeed + 20 + i), co = plantSet(x, 'corner', 0.05, kSynPlants, kSeed + 30 + i);
    note('PLANT', `${name}: 0.05 spike caught ${caughtAt(sp, K)} of ${kSynPlants} (${fmtSc(sp)}); 0.05 corner (over ${kCornerLen} samples) caught ${caughtAt(co, K)} of ${kSynPlants} (${fmtSc(co)})`);
  });
  LIMIT.forEach(([name, x], i) => {
    const st = plantSet(x, 'step', 0.05, kSynPlants, kSeed + 50 + i), st2 = plantSet(x, 'step', 0.2, kSynPlants, kSeed + 60 + i);
    note('LIMIT', `${name} (NOT judged; header): by itself scores up to ${top(x).toFixed(1)}; 0.05 steps caught ${caughtAt(st, K)} of ${kSynPlants} (scores ${fmtSc(st)}), 0.2 steps ${caughtAt(st2, K)} of ${kSynPlants}`);
  });
  const c2 = sig('band-limited saw C2');
  for (const [what, p] of [['the pitch stage off (ltp false)', { ltp: false }]]) {
    const ev = detectClicks(c2, SR, p);
    row(ev.length > 0, 'STAGE', `band-limited saw C2, clean, with ${what}: ${ev.length} event(s) — must read at least 1 (the stage is load-bearing)`);
  }
  const pk = peakOf(c2), ats = positions(n, 3, kSeed + 40);
  let lv = c2; [0.02, 0.05, 0.2].forEach((s, i) => { lv = plant(lv, ats[i], 'step', s * pk); });
  levelRow(row, 'LEVEL', 'band-limited saw C2 with steps of 0.02, 0.05 and 0.2 x peak', lv);
  let threw = false;
  try { const bad = Float64Array.from(c2); bad[40000] = NaN; detectClicks(bad, SR); } catch { threw = true; }
  row(threw, 'NONFINITE', 'a NaN in the input throws (a NaN render must never read as clean)');
  for (const [name, i] of [['band-limited saw C2', 1], ['band-limited saw C6', 2]]) {
    const x = sig(name), px = peakOf(x), unit = x.map(v => v / px), oats = positions(n, kSynPlants, kSeed + 10 + i);
    note('OLD', `the isolated-jump detector (tools/h2_frame_hold_check.mjs) on the ${name} at peak 1, the same 0.05 steps as MUSTFIRE: caught ${oldCatches(unit, oats, 0.05)} of ${kSynPlants}`);
  }
}

/* ---------------------------------------------------------------- the engine half */
async function engine(row, note, opt, eventsFile) {
  const { ROOT, jobFor, run } = await import('../h2/harness/harness.mjs');
  const bank = JSON.parse(readFileSync(join(ROOT, 'reference/scalpel/data/presets.json'), 'utf8')).presets;
  const bp = name => { const p = bank.find(q => q.name === name); if (!p) throw new Error(`no bank preset "${name}"`); return p.params; };
  const setCmd = ([k, v]) => (typeof v === 'string' ? ['sets', k, v] : ['set', k, v]);
  const N = Math.round(kSeconds * SR);
  const job = (name, params, notes) => jobFor({ engine: 'composed', name, sr: SR, seed: 1, cmds: [
    ...Object.entries(params).map(setCmd), ...Object.entries(kPreview).map(setCmd), ['snap'], ['set', 'poly', 8],
    ...notes.map(m => ['on', m, mtof(m), VEL]), ['render', N]] });
  // The bare saw is the engine's band-limited one: base 6 (the closed-form sine-to-saw) at morph 1
  // and blade ratio k 1, where its cap puts the harmonic at Nyquist 40 dB down (h2/engine/engine.h,
  // the member's mr). The plain saw, base 2, is computed naively (its reset lands on a whole
  // sample), so its reset moves by a fraction of a sample from period to period and no predictor
  // can follow it: it is printed as NAIVE, not judged (the research note).
  const SAW = { N: 1, depth: 0, b2on: 0, base: 6, morph: 1, k: 1 }, SINE = { N: 1, depth: 0, b2on: 0, base: 0 };
  const NAIVE = { N: 1, depth: 0, b2on: 0, base: 2 };
  const FIRE = [
    ['sine C4', SINE, [60], true], ['bare saw C2', SAW, [36], true], ['bare saw C6', SAW, [84], true], ['Crushed bells C4', bp('Crushed bells'), [60], true],
    ['Glass horde pad C4', bp('Glass horde pad'), [60], false], ['Breathing pad C3', bp('Breathing pad'), [48], false], ['Crushed bells chord C4 E4 G4 B4', bp('Crushed bells'), CHORD, false],
  ];
  const SMOOTH = [];
  for (const [nm, p] of [['sine', SINE], ['bare saw', SAW]]) for (let o = 1; o <= 7; o++) SMOOTH.push([`${nm} C${o}`, p, [12 * (o + 1)]]);
  const BANK = bank.map(p => [`chord ${p.name}`, p.params, CHORD]);
  const HELD = [];
  for (const [nm, p] of [['defaults', {}], ['Crushed bells', bp('Crushed bells')], ['Glass horde pad', bp('Glass horde pad')], ['Breathing pad', bp('Breathing pad')]])
    for (let o = 1; o <= 7; o++) HELD.push([`note ${nm} C${o}`, p, [12 * (o + 1)]]);
  const NAIVES = []; for (let o = 1; o <= 7; o++) NAIVES.push([`naive saw C${o}`, NAIVE, [12 * (o + 1)]]);
  const ALL = [...FIRE, ...SMOOTH, ...BANK, ...HELD, ...NAIVES];
  const rs = run(ALL.map(([name, p, notes]) => job(name, p, notes)), opt);
  const unknown = rs.filter(r => r.summary.unknownKeys).length;
  row(!unknown, 'KEYS', `the engine knew every key on ${rs.length - unknown} of ${rs.length} jobs`);
  const R = name => rs[ALL.findIndex(c => c[0] === name)];
  console.log(`engine: ${kSeconds} s at ${SR} Hz, seed 1, ${JSON.stringify(kPreview)}, poly 8, velocity ${VEL}; output channels; detector DEFAULTS (RATIFIED, ADR-227); k ${K}`);

  // ---- must-fire, the exceptions, B490
  const table = [];
  FIRE.forEach(([name, , , judged], i) => {
    const x = R(name).L, own = scores(x, SR), ownMax = own.score.reduce((m, v) => Math.max(m, v), 0);
    const rd = Object.fromEntries(kSizes.map((s, j) => [s, plantSet(x, 'step', s, kPlants, kSeed + 100 + 10 * i + j)]));
    const c05 = caughtAt(rd[0.05], K), all = kSizes.map(s => `${s} ${caughtAt(rd[s], K)}/${kPlants}`).join(', ');
    const med = q => { const v = q.map(z => z.score).sort((a, b) => a - b); return v[v.length >> 1]; };
    const text = `${name}: steps caught (size x peak: caught) ${all}; 0.05 scores ${fmtSc(rd[0.05])}`;
    if (judged) row(c05 === kPlants, 'MUSTFIRE', text);
    else {
      const every = kSizes.find(s => caughtAt(rd[s], K) === kPlants), bestS = kSizes.reduce((b, s) => (caughtAt(rd[s], K) > caughtAt(rd[b], K) ? s : b), kSizes[0]);
      note('EXCEPT', `${text}. NOT JUDGED: by itself this patch reads ${eventsOf(own, SR).length} event(s) and scores up to ${ownMax.toFixed(1)}, against a median of ${med(rd[0.05]).toFixed(1)} for a 0.05 step; ${every ? `caught every time from ${every} x peak` : `caught every time at none of the sizes tried (best: ${bestS} x peak, ${caughtAt(rd[bestS], K)} of ${kPlants})`}`);
    }
    const sp = plantSet(x, 'spike', 0.05, kSmallPlants, kSeed + 200 + i), co = plantSet(x, 'corner', 0.05, kSmallPlants, kSeed + 300 + i);
    const sp2 = plantSet(x, 'spike', 0.2, kSmallPlants, kSeed + 400 + i), co2 = plantSet(x, 'corner', 0.2, kSmallPlants, kSeed + 500 + i);
    note('PLANT', `${name}: spikes caught ${caughtAt(sp, K)}/${kSmallPlants} at 0.05 and ${caughtAt(sp2, K)}/${kSmallPlants} at 0.2; corners (over ${kCornerLen} samples) ${caughtAt(co, K)}/${kSmallPlants} at 0.05 and ${caughtAt(co2, K)}/${kSmallPlants} at 0.2`);
    table.push([name, rd[0.05], ownMax]);
  });
  {
    // the first difference (click_detector.mjs header, 2) is load-bearing: the margin between the
    // weakest 0.05 step and the patch's own largest score, on Crushed bells C4, with and without it
    const x = R('Crushed bells C4').L, margin = params => {
      const own = scores(x, SR, params).score.reduce((m, v) => Math.max(m, v), 0);
      return Math.min(...plantSet(x, 'step', 0.05, kPlants, kSeed + 120, params).map(q => q.score)) / own;
    };
    const mOn = margin({}), mOff = margin({ diff: false });
    row(mOn > mOff, 'STAGE', `Crushed bells C4: the weakest 0.05 step over the patch's own largest score, ${mOn.toFixed(2)} predicting the difference, ${mOff.toFixed(2)} predicting the sample (diff false) — must be larger with it (the stage is load-bearing)`);
  }
  {
    const x = R('Crushed bells C4').L, rd = positions(x.length, kPlants, kSeed + 600).map(at => plantReading(x, at, 'step', 0.05));
    row(caughtAt(rd, K) === kPlants, 'B490', `Crushed bells C4, steps of 0.05 of FULL SCALE (${(0.05 / peakOf(x)).toFixed(2)} x its peak ${peakOf(x).toFixed(3)}): caught ${caughtAt(rd, K)} of ${kPlants} (scores ${fmtSc(rd)})`);
  }

  // ---- must-not-fire: the smooth engine set
  let smoothMax = 0;
  const smoothScores = [];
  for (const [name] of SMOOTH) for (const c of ['L', 'R']) {
    const res = scores(R(name)[c], SR), ev = eventsOf(res, SR), mx = res.score.reduce((m, v) => Math.max(m, v), 0);
    smoothMax = Math.max(smoothMax, mx); smoothScores.push(res);
    if (ev.length) row(false, 'MUSTNOT', `${name} ${c}: ${ev.length} event(s) at ${ev.slice(0, 5).map(q => `${q.sample} (${q.score.toFixed(1)})`).join(', ')}`);
  }
  {
    const res = scores(R('bare saw C1').L, SR, { band: 1 }), ev = eventsOf(res, SR, { band: 1 });
    row(ev.length > 0, 'STAGE', `bare saw C1, clean, with the analysis band open (band 1): ${ev.length} event(s) — must read at least 1 (the stage is load-bearing)`);
  }
  row(smoothScores.every(q => eventsOf(q, SR).length === 0), 'MUSTNOT', `the sine and the bare saw held at C1 to C7, both channels (${SMOOTH.length * 2} channels): no event at k ${K}; largest score ${smoothMax.toFixed(2)}`);

  // ---- the naive saw (printed, NOT judged; the SAW comment above)
  note('NAIVE', `the plain saw (base 2, computed naively), held, left channel, k ${K}: ` + NAIVES.map(([name]) => {
    const res = scores(R(name).L, SR); return `${name.slice(10)} ${eventsOf(res, SR).length} event(s), up to ${res.score.reduce((m, v) => Math.max(m, v), 0).toFixed(0)}`;
  }).join('; '));

  // ---- the bank and the held notes: events by class (printed)
  const classify = (name, notes, ev, low) => {
    const lags = notes.flatMap(m => [0.5, 1, 2].map(t => t * SR / mtof(m)));
    // the most crowded run of kTickPhases tick phases (circular): the analysis band delays an
    // event by a few samples, so the run is found, not assumed to start at phase 0
    const inRun = (q, a) => ((q.sample - a) % kTick + kTick) % kTick < kTickPhases;
    let best = 0, at = 0;
    for (let a = 0; a < kTick; a++) { const c = ev.filter(q => inRun(q, a)).length; if (c > best) { best = c; at = a; } }
    const tickLocked = ev.length >= kTickMin && best * 2 >= ev.length;
    return ev.map(q => {
      if (tickLocked && inRun(q, at)) return 'tick';
      if (low.some(f => f.sample !== q.sample && lags.some(T => Math.abs(Math.abs(f.sample - q.sample) - T) <= 0.03 * T + 2))) return 'period';
      return 'isolated';
    });
  };
  const sets = { bank: BANK, held: HELD };
  const dump = [], sweep = { bank: kSweep.map(() => 0), held: kSweep.map(() => 0), smooth: kSweep.map(() => 0) };
  smoothScores.forEach(q => kSweep.forEach((k, j) => { sweep.smooth[j] += eventsOf(q, SR, { k }).length; }));
  const totals = {};
  for (const [set, cases] of Object.entries(sets)) {
    const t = totals[set] = { renders: 0, tick: 0, period: 0, isolated: 0 };
    for (const [name, , notes] of cases) {
      const cls = { tick: 0, period: 0, isolated: 0 }, firsts = [];
      for (const c of ['L', 'R']) {
        const x = R(name)[c], res = scores(x, SR), ev = eventsOf(res, SR), low = eventsOf(res, SR, { k: 6 });
        kSweep.forEach((k, j) => { sweep[set][j] += eventsOf(res, SR, { k }).length; });
        classify(name, notes, ev, low).forEach((k, j) => {
          cls[k]++;
          const q = ev[j];
          dump.push({ render: name, channel: c, sample: q.sample, seconds: q.sample / SR, tickPhase: q.sample % kTick, size: q.size, sizeOverPeak: q.size / peakOf(x), score: q.score, class: k });
          if (firsts.length < 3) firsts.push(`${c}${q.sample} ${q.score.toFixed(0)} ${k}`);
        });
      }
      const nEv = cls.tick + cls.period + cls.isolated;
      if (nEv) { t.renders++; note(set.toUpperCase(), `${name}: ${nEv} event(s): tick ${cls.tick}, period ${cls.period}, isolated ${cls.isolated} (first: ${firsts.join('; ')})`); }
      t.tick += cls.tick; t.period += cls.period; t.isolated += cls.isolated;
    }
    note(set.toUpperCase(), `${cases.length} renders, both channels, k ${K}: ${t.renders} read events; tick ${t.tick}, period ${t.period}, isolated ${t.isolated}`);
  }
  if (eventsFile) { writeFileSync(eventsFile, JSON.stringify(dump, null, 1)); note('EVENTS', `${dump.length} events written to ${eventsFile}`); }

  // ---- level invariance on the engine
  {
    const x = R('Crushed bells C4').L, at = positions(x.length, 1, kSeed + 700)[0];
    levelRow(row, 'LEVEL', 'Crushed bells C4 with a 0.05 step', plant(x, at, 'step', 0.05 * peakOf(x)));
    levelRow(row, 'LEVEL', 'the Screamer chord (events of its own)', R('chord Screamer').L);
  }

  // ---- the old detector on the same Crushed bells steps
  for (const name of ['Crushed bells C4', 'Crushed bells chord C4 E4 G4 B4']) {
    const i = FIRE.findIndex(f => f[0] === name), x = R(name).L, pk = peakOf(x), unit = x.map(v => v / pk);
    const parts = kSizes.map((s, j) => {
      const ats = positions(x.length, kPlants, kSeed + 100 + 10 * i + j);
      return `${s}: ${oldCatches(x, ats, s * pk)}/${kPlants} as rendered, ${oldCatches(unit, ats, s)}/${kPlants} at peak 1`;
    });
    note('OLD', `the isolated-jump detector (tools/h2_frame_hold_check.mjs, unchanged) on ${name}, the same step plants: ${parts.join('; ')}`);
  }

  // ---- the tables
  console.log(`TABLE  0.05 x peak steps caught (of ${kPlants}) against k: k = ${kSweep.join(' / ')}`);
  for (const [name, rd, ownMax] of table) console.log(`TABLE    ${name.padEnd(34)} ${kSweep.map(k => String(caughtAt(rd, k)).padStart(2)).join(' / ')}   (the patch alone scores up to ${ownMax.toFixed(1)})`);
  console.log(`TABLE  events against k: k = ${kSweep.join(' / ')}`);
  console.log(`TABLE    smooth set (${SMOOTH.length * 2} channels)            ${sweep.smooth.join(' / ')}`);
  console.log(`TABLE    bank chords (${BANK.length * 2} channels)          ${sweep.bank.join(' / ')}`);
  console.log(`TABLE    held notes (${HELD.length * 2} channels)            ${sweep.held.join(' / ')}`);
}

export async function main(argv) {
  let red = 0;
  const row = (ok, tag, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(9)} ${text}`); if (!ok) red++; };
  const note = (tag, text) => console.log(`NOTE  ${tag.padEnd(9)} ${text}`);
  const bi = argv.indexOf('--bin'), ei = argv.indexOf('--events');
  if (argv.includes('--synthetic')) synthetic(row, note);
  else if (bi >= 0) await engine(row, note, { bin: argv[bi + 1] }, ei >= 0 ? argv[ei + 1] : null);
  else throw new Error('usage: --synthetic | --bin <h2_harness_render> [--events <file>]');
  console.log(`click_detector_check: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; ${argv.includes('--synthetic') ? 'the synthetic half' : 'the engine half'}; k ${K} and every number RATIFIED (ADR-227), ADR-225 item 12)`);
  return red ? 1 : 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  main(process.argv.slice(2)).then(c => { process.exitCode = c; }, e => {
    console.error(String(e && e.stack || e));
    console.log(`click_detector_check: RED (the run broke: ${String(e && e.message || e).split('\n')[0]})`);
    process.exitCode = 2;
  });
}
