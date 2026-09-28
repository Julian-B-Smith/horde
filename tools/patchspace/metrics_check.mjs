/*
 * metrics_check.mjs — validates tools/patchspace/metrics.mjs (B316 P2) on CONSTRUCTED
 * signals, each metric with a must-read-zero and a must-read-high control (L0032, and
 * the detector-shares-assumption trap: a click detector that also fires on a plain saw's
 * periodic jumps, or a roughness that reads a lone sine as rough, would "confirm" any
 * patch). Plus three ENGINE controls through the gauntlet's own measurement path.
 * WIRED: ./verify fast.
 * HYPERSAW, 2026-09-27, ROADMAP B316 (records PR #810, branch lead-records-122).
 *
 * THE METRICS ARE MEASUREMENTS, NOT GATES. This check gates only that each metric reads
 * what it claims on signals whose answer is known by construction — never a patch's value.
 *
 * Signals at 48 kHz, 1 s: sine, band-limited saw/square (additive, below 20/24 kHz), naive
 * (sampled, aliasing) square and saw, seeded white noise (mulberry32), a sine with a step,
 * planted NaN/Inf, tone pairs a minor second and a fifth apart, a missing-fundamental stack.
 * ENGINE controls (docs/design/scalpel-horde-engine.js via gauntlet.mjs measure()):
 *   E1 must-read-zero: blades off, one member, sine base — a pure sine through the whole
 *      engine: aliasDb -120, roughness ~0, rootPresence 1, no clicks.
 *   E2 aliasing must-read-HIGHER: a hard sync saw blade at E5, band-limit off and no
 *      oversampling (aa 0, os 1) against the same patch clean (aa 1, os 2).
 *   E3 CPU must-read-HIGHER: nine members and two blades at 2x oversampling against one
 *      member, no blades, 1x — min of three timings each; the ratio is asserted (>= 2),
 *      never an absolute time (CPU is noisy; CI machines differ).
 * ~3 s. By hand: node tools/patchspace/metrics_check.mjs (exit 1 on any red row).
 */
import * as M from './metrics.mjs';
import { measure } from './gauntlet.mjs';
import { loadSpace, render, SR as ESR } from './space.mjs';

let red = 0;
const row = (ok, id, text) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${id.padEnd(4)} ${text}`); if (!ok) red++; };
const SR = 48000, n = 48000, TAU = 2 * Math.PI;
const gen = f => { const x = new Float64Array(n); for (let i = 0; i < n; i++) x[i] = f(i / SR, i); return x; };
const sine = (f, a = 1) => gen(t => a * Math.sin(TAU * f * t));
const blSaw = f => gen(t => { let s = 0; for (let h = 1; h * f < 20000; h++) s += (2 / Math.PI) * ((h % 2) ? 1 : -1) * Math.sin(TAU * h * f * t) / h; return s; });
const blSq = f => gen(t => { let s = 0; for (let h = 1; h * f < 24000; h += 2) s += (4 / Math.PI) * Math.sin(TAU * h * f * t) / h; return s; });
const nvSq = f => gen(t => (Math.sin(TAU * f * t) >= 0 ? 1 : -1));
const nvSaw = f => gen(t => 2 * ((f * t) % 1) - 1);
let st = 0x1234;
const rnd = () => { st |= 0; st = (st + 0x6D2B79F5) | 0; let t = Math.imul(st ^ (st >>> 15), 1 | st); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
const noise = gen(() => rnd() * 2 - 1);
const add = (x, y) => x.map((v, i) => v + y[i]);
const S = x => M.spectrum(x, SR);
const f4 = x => (Number.isFinite(x) ? +x.toPrecision(4) : x);

/* M1 non-finite */
{
  const x = sine(220), y = sine(220); y[100] = NaN; y[2000] = NaN; y[9000] = NaN; y[30] = Infinity; y[40] = -Infinity;
  row(M.nonFinite(x, x) === 0 && M.nonFinite(y, x) === 5, 'M1', `nonFinite: clean sine ${M.nonFinite(x, x)} (must be 0); 3 NaN + 2 Inf planted ${M.nonFinite(y, x)} (must be 5)`);
}
/* M2 DC */
{
  const a = M.dc(sine(220), sine(220)).dc, b = M.dc(sine(220).map(v => v + 0.1), sine(220)).dc;
  row(a < 1e-9 && Math.abs(b - 0.1) < 1e-9, 'M2', `dc: sine ${a.toExponential(1)} (must be ~0); sine + 0.1 ${f4(b)} (must be 0.1)`);
}
/* M3 level: crest, RMS, LUFS-like (BS.1770: a 997 Hz full-scale sine in both channels reads 0.0 LKFS) */
{
  const s1k = sine(997), lv = M.level(s1k, s1k, SR), sq = M.level(nvSq(220), nvSq(220), SR), q = M.level(sine(997, 0.1), sine(997, 0.1), SR);
  row(Math.abs(lv.lufs) < 0.15 && Math.abs(lv.crestDb - 3.0103) < 0.02 && Math.abs(sq.crestDb) < 0.01 && Math.abs(q.lufs + 20) < 0.15 && Math.abs(q.rmsDb + 23.01) < 0.02, 'M3',
    `level: 997 Hz full-scale stereo LUFS ${f4(lv.lufs)} (must be 0 ± 0.15), crest ${f4(lv.crestDb)} dB (3.01); square crest ${f4(sq.crestDb)} dB (0); -20 dB sine LUFS ${f4(q.lufs)}, RMS ${f4(q.rmsDb)} dBFS (-23.01)`);
}
/* M4 silence */
{
  const z = new Float64Array(n), a = M.silence(z, z).silent, b = M.silence(sine(220, 1e-5), sine(220, 1e-5)).silent, c = M.silence(sine(220, 1e-3), sine(220, 1e-3)).silent;
  row(a && b && !c, 'M4', `silence: zeros ${a}, -100 dB sine ${b} (must be silent); -60 dB sine ${c} (must not be)`);
}
/* M5 clicks: periodic jumps are NOT clicks; a one-off step is */
{
  const x = sine(220, 0.5); for (let i = 20000; i < n; i++) x[i] += 0.3;
  const a = M.clicks(sine(220)), b = M.clicks(blSaw(220)), c = M.clicks(nvSaw(220)), d = M.clicks(x);
  row(a.clicks === 0 && b.clicks === 0 && c.clicks === 0 && d.clicks >= 1 && d.worstDb > 20, 'M5',
    `clicks: sine ${a.clicks}, band-limited saw ${b.clicks}, naive saw ${c.clicks} (periodic jumps: all must be 0); sine + one step ${d.clicks} frames, worst ${f4(d.worstDb)} dB (must be >= 1, > 20 dB)`);
}
/* M6 flatness */
{
  const a = M.flatness(S(sine(220))), b = M.flatness(S(noise));
  row(a < 1e-6 && b > 0.9, 'M6', `flatness: sine ${a.toExponential(1)} (must be ~0); white noise ${f4(b)} (must be > 0.9)`);
}
/* M7 root presence */
{
  const note = 220, a = M.root(S(sine(note)), note), b = M.root(S(blSaw(note)), note), c = M.root(S(sine(note * Math.SQRT2)), note);
  const d = M.root(S(gen(t => { let s = 0; for (let h = 2; h <= 10; h++) s += Math.sin(TAU * h * note * t) / h; return s; })), note);
  row(a.rootPresence > 0.999 && a.rootInterval === 0 && b.rootPresence > 0.999 && b.rootInterval === 0 && c.rootPresence < 0.01 && c.rootInterval === 6 && d.rootPresence > 0.9, 'M7',
    `root: sine at the note ${f4(a.rootPresence)} (Δ${a.rootInterval} st), saw ${f4(b.rootPresence)} (Δ${b.rootInterval}) (must be 1, Δ0); sine a tritone up ${f4(c.rootPresence)} (Δ${c.rootInterval}) (must be ~0, Δ6); missing fundamental (h 2-10) ${f4(d.rootPresence)} (must be > 0.9)`);
}
/* M8 roughness (Sethares) */
{
  const a = M.roughness(S(sine(440))).roughness, m2 = M.roughness(S(add(sine(440, .5), sine(466.16, .5)))).roughness, p5 = M.roughness(S(add(sine(440, .5), sine(659.26, .5)))).roughness;
  row(a === 0 && m2 > 0.05 && p5 < 0.005 && m2 > 20 * p5, 'M8', `roughness: one sine ${a} (must be exactly 0); minor second ${f4(m2)} (must be > 0.05); fifth ${f4(p5)} (must be < 0.005); ratio ${f4(m2 / p5)} (must be > 20)`);
}
/* M9 aliasing */
{
  const a = M.aliasing(S(blSq(1760)), S(blSq(1760))), b = M.aliasing(S(sine(440)), S(sine(440))), c = M.aliasing(S(nvSq(1760)), S(blSq(1760)));
  row(a.aliasDb === -120 && b.aliasDb === -120 && c.aliasDb > -25, 'M9', `aliasing: band-limited square vs itself ${a.aliasDb} dB, sine vs itself ${b.aliasDb} dB (must be -120); naive square at 1760 Hz vs band-limited ${f4(c.aliasDb)} dB (must be > -25)`);
}
/* M10 cpuFraction arithmetic */
{
  const a = M.cpuFraction([[48000, 5e8]], 48000), b = M.cpuFraction([[4800, 1e7], [4800, 3e7], [4800, 2e7]], 48000);
  row(Math.abs(a - 0.5) < 1e-12 && Math.abs(b - 0.2) < 1e-12, 'M10', `cpuFraction: 0.5 s per 1 s ${a} (must be 0.5); median of 0.1/0.3/0.2 ${f4(b)} (must be 0.2)`);
}

/* ---- engine controls */
const D = loadSpace().defaults;
{
  const zero = Object.assign({}, D, { b1on: 0, b2on: 0, N: 1, base: 0, detune: 0, K: 0 });
  const m = measure(zero, 1);
  row(m.aliasDb === -120 && m.roughness < 1e-3 && m.rootPresence > 0.999 && m.clicks === 0 && m.nonFiniteA === 0, 'E1',
    `engine must-read-zero (blades off, N 1, sine): aliasDb ${m.aliasDb}, roughness ${f4(m.roughness)}, rootPresence ${f4(m.rootPresence)}, clicks ${m.clicks}`);
}
{
  const sync = Object.assign({}, D, { b1on: 1, w: 0.5, mode: 0, hot: 2, k: 7.3, hard: 0, depth: 1, N: 1, detune: 0, K: 0, b2on: 0 });
  const raw = measure(Object.assign({}, sync, { aa: 0, os: 1 }), 1).aliasDb, clean = measure(Object.assign({}, sync, { aa: 1, os: 2 }), 1).aliasDb;
  row(raw > clean + 6, 'E2', `engine aliasing: hard sync saw blade raw (aa 0, os 1) ${f4(raw)} dB vs clean (aa 1, os 2) ${f4(clean)} dB (raw must read >= 6 dB higher)`);
}
{
  const t = p => { let best = Infinity; for (let k = 0; k < 3; k++) best = Math.min(best, M.cpuFraction(render(p, { n: 12000, ev: [[0, 'on', 57, 0.8]] }, { seed: 1, timing: true, block: 512 }).timing, ESR)); return best; };
  const heavy = t(Object.assign({}, D, { N: 9, b2on: 1, os: 2, b1on: 1 })), light = t(Object.assign({}, D, { N: 1, b1on: 0, b2on: 0, os: 1 }));
  row(heavy / light >= 2, 'E3', `engine CPU: N 9 + two blades at 2x ${(100 * heavy).toFixed(1)}% vs N 1, no blades, 1x ${(100 * light).toFixed(1)}% of real time per voice — ratio ${f4(heavy / light)} (must be >= 2; min of 3 timings, noisy by nature)`);
}
console.log(`metrics_check: ${red ? red + ' RED' : 'GREEN'} — 10 metric rows on constructed signals + 3 engine controls (metrics are measurements, not gates)`);
process.exit(red ? 1 : 0);
