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
 * B345 ROWS (2026-09-28; the human: "Let's fix the metrics first, then re-fit"), one per change:
 *   M11  aliasing reads the same at 0.25 / 0.5 / 1.0 / 1.45 s (tolerance 1 dB) on a steady naive saw
 *        and on naive saws gliding 2 semitones/s; the additive saw reads -120 at every length.
 *   M11c CONTROL: the same glide compared after averaging the whole window (the pre-B345 order)
 *        spreads by ~10 dB — the invariant can fail.
 *   M12  must-read-zero: band-limited seeded noise against an INDEPENDENT realisation of itself
 *        (worst 0.25 s window, the page's rule) reads clean; M12h must-read-high: a tone planted
 *        in the test only, 15 dB under the noise, reads in every window. INFO M12i: folded white
 *        noise (+6 dB in band) still reads (folding, not tonal: the stated limit).
 *   M13  a silent (subnormal) window is not measured; a quiet naive saw still reads aliased.
 *   N1-N4 noiseDb: lines read below -60 dB (sine, saws, beating pairs, an inharmonic cloud); white
 *        noise reads 0 dB; saw + noise reads the known share within 2 dB; the same at any window
 *        length. INFO N3i: its measured limits (A1, a dense supersaw).
 * B346 ROWS (2026-09-29; the human: "I think we should try to build the cleanest system we can muster"), the
 * os-convergence estimator (metrics.mjs aliasConvergence) on constructed renders at 1x..16x:
 *   X1   must-read-zero: a band-limited saw. X2 must-read-high, converging and explained: naive saws at A3, E5, A6.
 *   X3   must-read-zero: independent noise renders at every rate. X4 must-read-high: a tone 15 dB under noise at 1x only.
 *   X5   a rate-dependent feedback loop (sine feedback FM, the engine's loop form) reads as dynamics, not folding.
 *   X6   dense partials over a noise floor read where B345's floor hid them.
 *   X7   the lead's controls (2026-09-29, from B350's red T10): naive saws at E5 and A3 under a 30-cent 5 Hz vibrato
 *        and a 2 st/s glide read aliased within 3 dB of steady; band-limited on the same paths read clean.
 *        X7c CONTROL: B345's aliasing() under-reads the A3 vibrato by >= 10 dB (the smear is real).
 *   X8   a rate-dependent level (1.5 dB louder at 1x) is not folding. INFO X2i: a naive saw at A1.
 * B360 ROWS (2026-09-29; the human: "I think noisy and rough should just label; some patches want
 * noisy or rough"), gauntlet.mjs's own classification (failures()/incoherence()/labels()) on
 * constructed records, not rendered patches:
 *   G1   a noisy-but-otherwise-clean record is healthy (failures and incoherence both empty) and
 *        carries the noisy label. G2 an aliased record still fails (aliasing was not moved).
 *        G1c CONTROL: a planted pre-B360 failures() that still gates noise DOES flag G1's record —
 *        the "healthy" assertion is not vacuous.
 * ~8 s. By hand: node tools/patchspace/metrics_check.mjs (exit 1 on any red row).
 */
import * as M from './metrics.mjs';
import { measure, failures, incoherence, labels, THRESH } from './gauntlet.mjs';
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
const CONV_CLEAN = M.CONV_CLEAN_DB;
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

/* ---- B345 (2026-09-28): the aliasing fixes and the noise measure, each with its controls */
const info = (id, text) => console.log(`INFO  ${id.padEnd(4)} ${text}`);
/* signals over 1.5 s at a rate `r`: a naive or additive saw on a pitch path (semitones/s from f0),
   so the 4x-rate reference is the same generator (for the additive saw: nothing above 20 kHz, so
   nothing folds at either rate). The additive saw uses the angle-addition recurrence. */
const DUR = 1.5;
const glideSaw = (kind, f0, stPerS, r) => {
  const N = Math.round(DUR * r), x = new Float64Array(N); let ph = 0;
  for (let i = 0; i < N; i++) {
    const f = f0 * Math.pow(2, stPerS * (i / r) / 12);
    if (kind === 'naive') x[i] = 2 * ph - 1;
    else { const s1 = Math.sin(TAU * ph), c1 = Math.cos(TAU * ph); let s = s1, c = c1, acc = 0;
      for (let h = 1; h * f < 20000; h++) { acc += ((h & 1) ? 1 : -1) * s / h; const t = s * c1 + c * s1; c = c * c1 - s * s1; s = t; } x[i] = (2 / Math.PI) * acc; }
    ph += f / r; ph -= Math.floor(ph);
  }
  return x;
};
const LENS = [0.25, 0.5, 1.0, 1.45], A0 = Math.round(0.05 * SR);
/* aliasing of test (at SR) against ref (at 4·SR, a 4x FFT so the bins and frame starts match) over [0.05 s, 0.05 s + len) */
const al = (t, r, len, pre) => { const b = A0 + Math.round(len * SR), St = M.spectrum(t.subarray(A0, b), SR, 8192), Sr = M.spectrum(r.subarray(4 * A0, 4 * b), 4 * SR, 32768);
  /* `pre`: the PRE-B345 ORDER — average the whole window first, compare once (a single "frame"
     that is the Welch mean) — kept here only as the control that proves M11 can fail */
  return (pre ? M.aliasing(Object.assign({}, St, { F: [St.P] }), Object.assign({}, Sr, { F: [Sr.P] })) : M.aliasing(St, Sr)).aliasDb; };
const spread = v => Math.max(...v) - Math.min(...v), f1 = v => v.toFixed(1);
/* M11 ALIASING READS THE SAME AT ANY WINDOW LENGTH (B342(1)): 0.25 / 0.5 / 1.0 / 1.45 s, tolerance 1 dB,
   on a steady naive saw and on naive saws gliding a steady 2 semitones a second (partials that move
   slower than one window but far within the long one: the case that went blind) */
{
  const cases = [['naive saw E5, steady', 'naive', 659.26, 0], ['naive saw A3, gliding 2 st/s', 'naive', 220, 2], ['naive saw E5, gliding 2 st/s', 'naive', 659.26, 2]];
  const rows = cases.map(([name, kind, f0, g]) => { const t = glideSaw(kind, f0, g, SR), r = glideSaw('saw', f0, g, 4 * SR); return { name, v: LENS.map(L => al(t, r, L)), pre: LENS.map(L => al(t, r, L, true)) }; });
  const clean = (() => { const t = glideSaw('saw', 220, 2, SR), r = glideSaw('saw', 220, 2, 4 * SR); return LENS.map(L => al(t, r, L)); })();
  row(rows.every(x => spread(x.v) <= 1 && Math.min(...x.v) > -40) && clean.every(v => v === -120), 'M11',
    `aliasing vs window length ${LENS.join(' / ')} s (spread must be <= 1 dB, reading > -40): ` + rows.map(x => `${x.name} ${x.v.map(f1).join(' / ')} (spread ${f1(spread(x.v))})`).join('; ') +
    `; additive saw A3 gliding ${clean.map(f1).join(' / ')} (must be -120 at every length)`);
  const g = rows[1];
  row(spread(g.pre) > 5, 'M11c', `CONTROL: the same glide compared AFTER averaging the whole window (the pre-B345 order) reads ${g.pre.map(f1).join(' / ')} dB — spread ${f1(spread(g.pre))} (must be > 5 dB: the invariant can fail, and did)`);
}
/* band-limited seeded noise: white at 4·SR through a 511-tap Blackman windowed-sinc at 20 kHz; its 1x
   version is the same stream decimated by 4 (nothing above 20 kHz, so nothing folds) */
const BLF = (() => { const L = 511, fc = 20000 / (4 * SR), h = new Float64Array(L), c = (L - 1) / 2; let s = 0;
  for (let i = 0; i < L; i++) { const t = i - c, w = 0.42 - 0.5 * Math.cos(TAU * i / (L - 1)) + 0.08 * Math.cos(2 * TAU * i / (L - 1)); h[i] = (t === 0 ? 2 * fc : Math.sin(TAU * fc * t) / (Math.PI * t)) * w; s += h[i]; } return h.map(v => v / s); })();
const blNoise4 = seed => { st = seed; const N = Math.round(DUR * 4 * SR), w = new Float64Array(N), y = new Float64Array(N); for (let i = 0; i < N; i++) w[i] = rnd() * 2 - 1;
  for (let i = 0; i < N; i++) { let a = 0; for (let j = 0; j < BLF.length && j <= i; j++) a += BLF[j] * w[i - j]; y[i] = a; } return y; };
const dec4 = x => { const y = new Float64Array(x.length / 4); for (let i = 0; i < y.length; i++) y[i] = x[4 * i]; return y; };
/* the page's rule: the worst 0.25 s window (hop 0.125 s) over a 1.45 s hold */
const worstWin = (t, r) => { let w = -Infinity; for (let a = A0; a + 12000 <= A0 + Math.round(1.45 * SR); a += 6000) w = Math.max(w, M.aliasing(M.spectrum(t.subarray(a, a + 12000), SR, 8192), M.spectrum(r.subarray(4 * a, 4 * a + 48000), 4 * SR, 32768)).aliasDb); return w; };
/* M12 TWO RENDERS OF THE SAME NOISE ARE NOT FOLDING (B342(2)): the reference is ANOTHER realisation of the
   same band-limited process (what the 1x and 4x renders of a noise-like patch are); a planted tone in
   the test only, 15 dB under the noise, is folding by construction and must read */
{
  const a4 = blNoise4(0xB345), b4 = blNoise4(0x5B43), a1 = dec4(a4);
  const pw = x => { let s = 0; for (const v of x) s += v * v; return s / x.length; };
  const amp = Math.sqrt(2 * pw(a1) * Math.pow(10, -15 / 10)), withTone = a1.map((v, i) => v + amp * Math.sin(TAU * 3517 * i / SR));
  const indep = worstWin(a1, b4), same = worstWin(a1, a4), planted = worstWin(withTone, b4);
  const plantedAll = []; for (let a = A0; a + 12000 <= A0 + Math.round(1.45 * SR); a += 6000) plantedAll.push(M.aliasing(M.spectrum(withTone.subarray(a, a + 12000), SR, 8192), M.spectrum(b4.subarray(4 * a, 4 * a + 48000), 4 * SR, 32768)).aliasDb);
  row(indep <= -90 && same === -120, 'M12', `aliasing, worst 0.25 s window over 1.45 s: band-limited noise vs an INDEPENDENT realisation of it ${f1(indep)} dB, vs its own stream ${f1(same)} dB (must be <= -90 and -120: decorrelation is not folding)`);
  row(Math.min(...plantedAll) > -20 && planted > -20, 'M12h', `aliasing: the same noise with a planted 3517 Hz tone 15 dB under it (test only) reads ${plantedAll.map(v => v.toFixed(0)).join(' ')} dB per window (every window must be > -20: its ~-15 dB share)`);
  /* INFORMATION: noise drawn per sample at EACH rate (the 1x floor 6 dB higher in band: folded noise)
     still reads — folding, but not tonal; the limit stated in metrics.mjs */
  st = 0x77; const w1 = gen(() => rnd() * 2 - 1); st = 0x78; const w4 = new Float64Array(4 * n); for (let i = 0; i < w4.length; i++) w4[i] = rnd() * 2 - 1;
  info('M12i', `folded white noise (drawn per sample at 48 kHz vs at 192 kHz: +6 dB in band) reads ${f1(M.aliasing(M.spectrum(w1.subarray(A0, A0 + 12000), SR, 8192), M.spectrum(w4.subarray(4 * A0, 4 * A0 + 48000), 4 * SR, 32768)).aliasDb)} dB over 0.25 s (not asserted: folded noise is folding, but not tonal)`);
}
/* M13 A SILENT WINDOW IS NOT MEASURED (edge#145's sweep read 0 dB: subnormal samples against an exact-zero reference) */
{
  const sub = gen(t => 1e-40 * Math.sin(TAU * 659.26 * t)), zero = new Float64Array(4 * n);
  const quiet = gen(t => 3e-4 * (2 * ((659.26 * t) % 1) - 1)), quietRef = glideSaw('saw', 659.26, 0, 4 * SR).map(v => 3e-4 * v);
  const a = M.aliasing(M.spectrum(sub.subarray(0, 12000), SR, 8192), M.spectrum(zero.subarray(0, 48000), 4 * SR, 32768)).aliasDb;
  const b = M.aliasing(M.spectrum(quiet.subarray(0, 12000), SR, 8192), M.spectrum(quietRef.subarray(0, 48000), 4 * SR, 32768)).aliasDb;
  row(a === -120 && b > -30, 'M13', `aliasing on silence: a 1e-40 sine vs an all-zero reference ${a} dB (must be -120, not 0); a naive saw at -75 dBFS vs its clean reference ${f1(b)} dB (must still read aliased, > -30)`);
}
/* N1-N4 noiseDb, the aperiodic share (B345's proposed replacement for flatness) */
{
  const nz = x => M.aperiodic(S(x)).noiseDb;
  const inBand = x => { const Sx = S(x); let s = 0; for (let k = Math.ceil(50 / Sx.binHz); k <= Math.floor(16000 / Sx.binHz); k++) s += Sx.P[k]; return s; };
  const mix = (tone, shareDb, seed) => { st = seed; const w = gen(() => rnd() * 2 - 1), pt = inBand(tone), pn = inBand(w), fr = Math.pow(10, shareDb / 10); const g = Math.sqrt(fr / (1 - fr) * pt / pn); return add(tone, w.map(v => v * g)); };
  const cloud = (() => { st = 0x9; const parts = Array.from({ length: 40 }, () => [200 + 3800 * rnd(), TAU * rnd()]); return gen(t => parts.reduce((s, [f, p]) => s + Math.sin(TAU * f * t + p), 0) / 6); })();
  const zeroRows = [['sine A3', nz(sine(220))], ['saw A1', nz(blSaw(55))], ['saw A3', nz(blSaw(220))], ['saw E5', nz(blSaw(659.26))], ['saws a minor second apart', nz(add(blSaw(220).map(v => v / 2), blSaw(233.08).map(v => v / 2)))],
    ['sines a minor second apart', nz(add(sine(440, .5), sine(466.16, .5)))], ['40 seeded inharmonic sines', nz(cloud)]];
  row(zeroRows.every(([, v]) => v <= -60), 'N1', 'noiseDb must-read-zero (lines, not noise; each must be <= -60 dB): ' + zeroRows.map(([k, v]) => `${k} ${f1(v)}`).join(', '));
  const wn = nz(noise);
  row(wn >= -1, 'N2', `noiseDb must-read-high: seeded white noise ${f1(wn)} dB (must be >= -1)`);
  const snr = [[220, -40], [220, -30], [220, -20], [220, -10], [659.26, -20]].map(([f, d], k) => ({ f, d, v: nz(mix(blSaw(f), d, 0x100 + k)) }));
  row(snr.every(x => Math.abs(x.v - x.d) <= 2), 'N3', 'noiseDb reads a known noise share (saw + white noise; within 2 dB): ' + snr.map(x => `${x.f === 220 ? 'A3' : 'E5'} ${x.d} -> ${f1(x.v)}`).join(', '));
  const lo = nz(mix(blSaw(55), -30, 0x200)), ens = nz([-25, -17, -8, 0, 8, 17, 25].map((c, k) => blSaw(220 * Math.pow(2, c / 1200)).map(v => v / 7)).reduce((a, b) => add(a, b)));
  info('N3i', `measured limits (not asserted): saw A1 + noise at -30 dB reads ${f1(lo)} (the floor sits on the lobe skirts at A1); a 7-voice ±25-cent supersaw at A3 reads ${f1(ens)} (partials closer than the resolution fill the valleys)`);
  const w20 = (() => { st = 0x300; const N = Math.round(DUR * SR), t = new Float64Array(N); for (let i = 0; i < N; i++) { let s = 0; for (let h = 1; h * 220 < 20000; h++) s += (2 / Math.PI) * ((h % 2) ? 1 : -1) * Math.sin(TAU * h * 220 * i / SR) / h; t[i] = s; }
    const pt = inBand(t.subarray(0, n)), w = new Float64Array(N); for (let i = 0; i < N; i++) w[i] = rnd() * 2 - 1; const pn = inBand(w.subarray(0, n)), g = Math.sqrt(0.01 / 0.99 * pt / pn); return t.map((v, i) => v + g * w[i]); })();
  const nl = LENS.map(L => M.aperiodic(M.spectrum(w20.subarray(A0, A0 + Math.round(L * SR)), SR)).noiseDb);
  row(spread(nl) <= 1, 'N4', `noiseDb vs window length ${LENS.join(' / ')} s, saw A3 + noise at -20 dB: ${nl.map(f1).join(' / ')} (spread ${f1(spread(nl))}, must be <= 1 dB)`);
}

/* ---- B346 (2026-09-29): the os-convergence estimator (metrics.mjs aliasConvergence), each class with its control.
   A constructed "render at os N" is the signal generated at N x 48 kHz and passed through ONE steep windowed-sinc
   low-pass (Blackman-Harris, 21 kHz, 96*N+1 taps) as it is decimated to 48 kHz; at N = 1 the same low-pass runs at
   48 kHz on what was sampled there (so it has folded, as the engine's os 1 has). Every render therefore has the same
   in-band response and no gain correction is needed. The source test's fine stream is the raw 8x generation (N = 1,
   so no decimation filter weights the images). Five renders at 1, 2, 4, 8, 16. A generator is built fresh per
   render, so a moving pitch runs the same path in seconds at every rate; noise is scaled by sqrt(N) so every rate
   draws the same power density (the same process, not louder folded noise: M12i's case). */
{
  const XD = 0.35, XA = 2400, XB = 14400;
  const firX = {};
  const firOf = os => { if (firX[os]) return firX[os]; const L = 96 * os + 1, c = (L - 1) / 2, fc = 21000 / (SR * os), h = new Float64Array(L); let s = 0;
    for (let i = 0; i < L; i++) { const t = i - c, x = TAU * i / (L - 1), w = 0.35875 - 0.48829 * Math.cos(x) + 0.14128 * Math.cos(2 * x) - 0.01168 * Math.cos(3 * x); h[i] = (t === 0 ? 2 * fc : Math.sin(TAU * fc * t) / (Math.PI * t)) * w; s += h[i]; }
    for (let i = 0; i < L; i++) h[i] /= s; return (firX[os] = h); };
  const decX = (x, os) => { const h = firOf(os), n = Math.floor(x.length / os), y = new Float64Array(n), c = (h.length - 1) / 2;
    for (let i = 0; i < n; i++) { const m = i * os; let a = 0; for (let j = 0; j < h.length; j++) { const k = m + c - j; if (k >= 0 && k < x.length) a += h[j] * x[k]; } y[i] = a; } return y; };
  /* mk(os, q) -> a fresh (t, i, rate) -> sample; returns the estimator's reading, and the B345 metric on the os-1 and
     os-4 renders beside it */
  const five = mk => { const S = {}, names = ['N', 'N2', 'N4', 'N8', 'N16']; let Sint = null;
    [1, 2, 4, 8, 16].forEach((os, q) => { const r = SR * os, N = Math.round(XD * r), raw = new Float64Array(N), fn = mk(os, q);
      for (let i = 0; i < N; i++) raw[i] = fn(i / r, i, r);
      S[names[q]] = M.spectrum(decX(raw, os).subarray(XA, XB), SR, 8192);
      if (os === 8) Sint = M.spectrum(raw.subarray(XA * 8, XB * 8), SR * 8, 8192 * 8); });
    return Object.assign(M.aliasConvergence(S, Sint, 1, SR), { b345: M.aliasing(S.N, S.N4).aliasDb }); };
  const fx = c => `${f1(c.excessDb)} dB (${c.cls}, conv ${f1(c.convDb)}, explained ${c.explainedShare === null ? '—' : c.explainedShare.toFixed(2)}; B345 ${f1(c.b345)})`;
  /* a saw on a pitch path (semitones as a function of seconds): 'naive' = 2*phase - 1; 'bl' = every harmonic under
     20 kHz at the instantaneous pitch, by the angle-addition recurrence */
  const sawX = (kind, f0, path) => () => { let ph = 0; return (t, i, r) => { const f = f0 * Math.pow(2, path(t) / 12); let y;
    if (kind === 'naive') y = 2 * ph - 1;
    else { const s1 = Math.sin(TAU * ph), c1 = Math.cos(TAU * ph); let s = s1, c = c1, acc = 0; for (let h = 1; h * f < 20000; h++) { acc += ((h & 1) ? 1 : -1) * s / h; const u = s * c1 + c * s1; c = c * c1 - s * s1; s = u; } y = (2 / Math.PI) * acc; }
    ph += f / r; ph -= Math.floor(ph); return y; }; };
  const STEADY = () => 0, VIB = t => 0.3 * Math.sin(TAU * 5 * t), GLIDE = t => 2 * t;
  const noiseX = seed => (os, q) => { st = seed * 100 + q; const g = Math.sqrt(os); return () => g * (rnd() * 2 - 1); };
  /* X1 must-read-zero: the band-limited saw is the same at every rate */
  const x1 = five(sawX('bl', 659.26, STEADY));
  row(x1.excessDb === -120 && x1.cls === 'clean', 'X1', `os-convergence: band-limited saw E5 reads ${fx(x1)} (must be -120, clean)`);
  /* X2 must-read-high AND converging AND explained: the naive saw folds, less at every doubling, from content the finer renders hold */
  const x2 = [220, 659.26, 1760].map(f => [f, five(sawX('naive', f, STEADY))]);
  row(x2.every(([, c]) => c.excessDb > -30 && c.cls === 'folding' && c.convDb >= 3 && c.explainedShare >= 0.9), 'X2',
    'os-convergence: naive saw ' + x2.map(([f, c]) => `${f} Hz ${fx(c)}`).join('; ') + ' (each must read > -30 dB, folding, converging >= 3 dB, explained >= 0.9)');
  info('X2i', `naive saw A1 (55 Hz) reads ${fx(five(sawX('naive', 55, STEADY)))} (harmonics 9.4 bins apart: the limit of this 8192-point grid; not asserted)`);
  /* X3 must-read-zero: independent noise renders, a new realisation at every rate, the same power density */
  const x3 = [11, 12, 13, 14, 15].map(seed => five(noiseX(seed)));
  row(x3.every(c => c.excessDb === -120 && c.cls === 'clean'), 'X3', `os-convergence: independent noise renders at every rate, five seeds: ${x3.map(c => f1(c.excessDb)).join(' / ')} dB (each must be -120, clean: the references disagree bin by bin, so the two-reference rule runs, and a chance dip must occur in BOTH)`);
  /* X4 must-read-high: a tone 15 dB under the noise, in the os-1 render only */
  const x4 = five((os, q) => { const w = noiseX(0x44)(os, q), amp = Math.sqrt(2 / 3 * Math.pow(10, -1.5)); return t => w() + (os === 1 ? amp * Math.sin(TAU * 3517 * t) : 0); });
  row(x4.excessDb > -20 && x4.cls !== 'clean', 'X4', `os-convergence: noise with a tone 15 dB under it in the os-1 render only reads ${fx(x4)} (must be > -20 dB and not clean)`);
  /* X5 a RATE-DEPENDENT FEEDBACK system: sine feedback FM with the engine's own loop (the phase pushed by beta times the
     mean of the last two outputs, one internal sample per step). At beta 0.6 the os-1 loop runs a limit cycle near a
     third of the rate that os 2 does not; at beta 2.2 it is chaotic at every rate. Neither is folding. */
  const fbX = beta => () => { let y1 = 0, y2 = 0, ph = 0; return (t, i, r) => { const y = Math.sin(TAU * (ph + beta * 0.5 * (y1 + y2))); y2 = y1; y1 = y; ph += 659.26 / r; ph -= Math.floor(ph); return y; }; };
  const x5a = five(fbX(0.6)), x5b = five(fbX(2.2));
  row(x5a.excessDb > -20 && x5a.cls === 'dynamics' && x5b.cls !== 'folding', 'X5', `os-convergence: sine feedback FM E5, beta 0.6 reads ${fx(x5a)} (must be > -20 dB, dynamics); beta 2.2 reads ${fx(x5b)} (must not be folding)`);
  /* X6 what B345's noise floor hid: seven naive saws ±25 cents at E5 (dense partials) over a noise floor (an independent
     realisation per rate, as a noise blade's would be) */
  const ensX = (os, q) => { const w = noiseX(0x66)(os, q), ph = new Float64Array(7), cs = [-25, -17, -8, 0, 8, 17, 25];
    return (t, i, r) => { let s = 0; for (let v = 0; v < 7; v++) { s += (2 * ph[v] - 1) / 7; ph[v] += 659.26 * Math.pow(2, cs[v] / 1200) / r; ph[v] -= Math.floor(ph[v]); } return s + 0.03 * w(); }; };
  const x6 = five(ensX);
  row(x6.cls === 'folding' && x6.excessDb >= x6.b345 + 3, 'X6', `dense partials over a noise floor (seven naive saws ±25 cents, E5): ${fx(x6)} (must be folding and read >= 3 dB above B345: the aliases between the partials count again)`);
  /* X7 THE MOVING PITCH (the lead, 2026-09-29, from B350's red T10: the listening page's "naive saw E5 vibrato" control read
     -27.2 dB on the B345 metric, under the ruled cut, where the steady saw reads -18.9). Naive saws at E5 and A3 with a
     30-cent vibrato at 5 Hz and with a 2 semitone/s glide must read aliased, within 3 dB of the same saw held steady;
     the band-limited saws on the same paths must read clean. */
  const moving = [];
  for (const [fn, f] of [['E5', 659.26], ['A3', 220]]) {
    const steady = five(sawX('naive', f, STEADY));
    for (const [pn, path] of [['vibrato', VIB], ['glide', GLIDE]]) moving.push({ id: `naive ${fn} ${pn}`, c: five(sawX('naive', f, path)), steady, bl: five(sawX('bl', f, path)) });
  }
  row(moving.every(m => m.c.cls === 'folding' && m.c.excessDb > -30 && Math.abs(m.c.excessDb - m.steady.excessDb) <= 3 && m.bl.excessDb <= CONV_CLEAN), 'X7',
    'os-convergence under a moving pitch: ' + moving.map(m => `${m.id} ${fx(m.c)} vs steady ${f1(m.steady.excessDb)}; band-limited ${f1(m.bl.excessDb)} (B345 ${f1(m.bl.b345)})`).join('; ') +
    ' (each naive must be folding, > -30 dB and within 3 dB of steady; each band-limited <= -60)');
  /* X8 a rate-dependent LEVEL is not folding: the band-limited saw 1.5 dB louder in the os-1 render than in every finer
     one (a gain that moves with os, as a loop or a smoother tuned per internal sample can make) */
  const x8 = five((os, q) => { const f = sawX('bl', 659.26, STEADY)(), g = os === 1 ? Math.pow(10, 1.5 / 20) : 1; return (t, i, r) => g * f(t, i, r); });
  row(x8.excessDb <= CONV_CLEAN && x8.cls === 'clean', 'X8', `os-convergence: a band-limited saw 1.5 dB louder at os 1 than at every finer rate reads ${fx(x8)} (must be <= -60, clean: a level is not folding)`);
  const a3v = moving.find(m => m.id === 'naive A3 vibrato');
  row(a3v.steady.b345 - a3v.c.b345 >= 10, 'X7c', `CONTROL: the B345 metric on the same A3 saw reads ${f1(a3v.steady.b345)} dB steady and ${f1(a3v.c.b345)} dB under the vibrato (must under-read by >= 10 dB: the smear the power difference answers is real)`);
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

/* ---- B360 (2026-09-29, human: "I think noisy and rough should just label; some patches want
   noisy or rough"): noiseDb and roughness LABEL a gauntlet.mjs record (labels()) and no longer
   push into failures() or incoherence(). aliasConvDb (failures()) and rootPresence (incoherence())
   are unmoved. Constructed records, not rendered patches: failures()/incoherence()/labels() are
   pure functions of the measured fields, so this tests gauntlet.mjs's own classification, not the
   DSP (that is E1-E3 above and the X rows). */
{
  const clean = { nonFiniteA: 0, nonFiniteB: 0, nonFiniteC: 0, silent: false, cpuVoiceNorm: 0.01, aliasConvDb: -120,
    clicks: 0, dcRatio: 0, rootPresence: 0.9, noiseDb: -10, roughness: 0.01 };
  const g1fail = failures(clean), g1inco = incoherence(clean), g1lab = labels(clean);
  row(g1fail.length === 0 && g1inco.length === 0 && g1lab.length === 1 && g1lab[0] === 'noisy', 'G1',
    `gauntlet classification: a noisy-but-otherwise-clean record (noiseDb ${clean.noiseDb} > THRESH ${THRESH.noiseDb}) is healthy (failures ${JSON.stringify(g1fail)}, incoherence ${JSON.stringify(g1inco)}) and carries the noisy label (${JSON.stringify(g1lab)})`);
  const aliased = Object.assign({}, clean, { aliasConvDb: -10, noiseDb: -90 });
  const g2fail = failures(aliased);
  row(g2fail.length === 1 && g2fail[0] === 'alias', 'G2',
    `gauntlet classification: an aliased record (aliasConvDb ${aliased.aliasConvDb} > THRESH ${THRESH.aliasConvDb}) still fails (${JSON.stringify(g2fail)})`);
  /* CONTROL: a planted pre-B360 failures() (noise still gates) must flag this same noisy-but-clean
     record — proving G1's "healthy" assertion is not vacuous, and that a regression back to gating
     noise (redenning P4's yield again) would be caught here first. */
  const preB360Failures = r => failures(r).concat(r.noiseDb !== null && r.noiseDb !== undefined && r.noiseDb > THRESH.noiseDb ? ['noisy'] : []);
  const planted = preB360Failures(clean);
  row(planted.includes('noisy'), 'G1c',
    `CONTROL: a planted pre-B360 failures() that still pushes 'noisy' DOES flag the same noisy-but-clean record (${JSON.stringify(planted)}) — the G1 assertion discriminates, it does not pass vacuously`);
}
console.log(`metrics_check: ${red ? red + ' RED' : 'GREEN'} — 10 metric rows + 9 B345 rows (aliasing window-length invariance, decorrelation, silence; noiseDb) + 9 B346 rows (the os-convergence estimator: clean, folding, independent noise, a planted tone, feedback dynamics, dense partials, vibrato and glide, a level) on constructed signals, 4 INFO rows, + 3 engine controls + 3 B360 gauntlet-classification rows (noisy-but-clean is healthy and labelled, aliased still fails, a planted pre-B360 control) (metrics are measurements, not gates)`);
process.exit(red ? 1 : 0);
