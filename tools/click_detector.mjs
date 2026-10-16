/*
 * click_detector.mjs — a level-relative click detector for any render, built for bright,
 * harmonically dense tones (ROADMAP B490; ADR-225 item 12).
 *
 * This file is the module and its CLI. Its oracle is tools/click_detector_check.mjs, which
 * ./verify fast runs on synthetic signals and ./verify full on the engine.
 *
 * THE METHOD IS RATIFIED (ADR-225 item 12): "a predictor from the preceding samples; a click is a
 * residual far over the local residual level; planted-step controls". ITS NUMBERS ARE RATIFIED (ADR-227, 2026-10-16; proposed here first)
 * (DEFAULTS below, and docs/research/2026-10-16-click-detector.md for how each was chosen). They
 * await the human's ratification; until then no number here is a ruling.
 *
 * WHY A NEW DETECTOR. The isolated-jump detector of tools/h2_frame_hold_check.mjs judges a
 * sample-to-sample step against the render's own roughness (3 x the local 99th percentile of the
 * steps, and an absolute floor of 0.02). On a bright patch the tone's own steps are large, so the
 * threshold rises with them: on Crushed bells it caught 0 of 10 planted steps up to 0.3 (B490).
 * Here a step is judged against what the samples before it PREDICT, so a bright tone's large but
 * predictable steps do not raise the bar.
 *
 * HOW IT WORKS, per channel (detectClicks takes one channel; the CLI runs each). Each choice
 * below was measured on 2026-10-16; the research note has the numbers.
 *   1. ANALYSIS BAND. The signal is low-passed at `band` x Nyquist (a causal Butterworth of order
 *      lpOrder; causal because a zero-phase filter rings before a step and lets the predictor see
 *      it coming). Why: what the pitch predictor (4) cannot cancel lives near Nyquist (no short
 *      fractional delay reproduces it), and on the engine's own band-limited saw held at C1 that
 *      residue read as events at every reset with the band open (scores to 24; 5 with the band at
 *      0.75). COST: a click with no energy below 0.75 x Nyquist (above 18 kHz at 48 kHz) is not
 *      seen, and every plant scores lower (Crushed bells C4's weakest 0.05 step: 33 open, 19 here).
 *   2. THE FIRST DIFFERENCE is what is predicted (`diff`): the predictor of the sample is the last
 *      sample plus the predicted difference, so it always carries a zero at DC. Why: a step then
 *      leaves ONE unpredicted sample. Predicting the sample itself, a step left a residual offset
 *      that lasted until the filters were refitted, and the pitch predictor (4) turned it into a
 *      box one period long; both raised the step's own local level until a 0.05 step on a saw at
 *      C6 scored 1 to 13 against 60 to 270 now.
 *   3. SHORT-TERM PREDICTOR. Order p, refitted every `hop` seconds on the `window` seconds BEFORE
 *      the block (so the block's own samples, and a click in it, never shape their prediction).
 *      Fitting: the autocorrelation method (Hann window, Levinson-Durbin) with white-noise
 *      correction `eta` (the autocorrelation's zero lag x (1 + eta)). Burg's method was tried
 *      first and REJECTED: on a pure band-limited tone it fits a filter of near-unit-circle poles,
 *      and its residual at a saw's reset was 588 times the tone's peak; the correction bounds the
 *      filter's gain (about 1 / eta in power). Each block's residual crossfades from the previous
 *      filter to the new one: a step in the coefficients would itself read as a click (it did, at
 *      every block, on a sine, before the crossfade).
 *   4. PITCH (LONG-TERM) PREDICTOR. A saw's reset or a blade's edge is a step the short-term
 *      predictor cannot see coming, but it recurs one period later. Every `ltpHop` seconds: the lag
 *      T is the shortest local peak of the normalised autocorrelation of the short-term residual
 *      (over the 2/fmin seconds before the block) within `octave` of the best, if the best reaches
 *      `rMin`; then 2*taps+1 taps on lags T-taps..T+taps (a fractional delay) are fitted to the
 *      differenced signal by least squares over the `fitLen` seconds before the block, and
 *      crossfaded across the block like the short-term filter. What it leaves is whitened by the
 *      short-term predictor (3) again. Without this stage every reset of a band-limited saw at C2
 *      reads as a click (the check's STAGE row shows it on every run). ONE period only: a chord's
 *      other notes, or detuned members that do not lock, are not followed (a cascade of stages
 *      was tried and did not help: the lag search on a mixture misleads it).
 *   5. THE RESIDUAL is, per sample, the smaller in magnitude of the short-term residual (3) and the
 *      pitch-then-short-term residual (4). So a sample is a click only if neither explains it, and
 *      the pitch predictor's replay of a click one period later does not read as a second click:
 *      the short-term residual there is ordinary.
 *   6. THE SCORE of a sample is |residual| over the local residual level: the RMS of the residual
 *      over `local` seconds centred on the sample, leaving out the sample's own footprint
 *      ([i - guard, i + p + guard]: its residual runs through the p-tap error filter after it) and
 *      that footprint's echo one pitch period later (where the pitch predictor replays it). The
 *      level is never taken below `floorDb` under the peak of the analysed signal (stages 1 and 2:
 *      the band-limited difference): a tone predicted almost perfectly leaves a residual near
 *      zero, against which the last 0.1 % of an imperfect cancellation would read as a click (the
 *      engine's band-limited saw at C1 did, at score 860, with the floor at -100 dB). Relative to
 *      the render's own peak, so scaling the render moves the floor with it. The cost: on a tone
 *      the predictors follow perfectly, the smallest event is k x floor, 1 % of that peak.
 *   7. A CLICK is a sample whose score exceeds k. Hits within `merge` seconds of the previous hit
 *      are one event; the event is reported at its highest-scoring sample.
 *
 * WHAT IT RETURNS. detectClicks(samples, sr, params) -> [{ sample, size, score }]: the sample index
 * (a few samples after the click itself: the analysis band is causal, and its delay at 48 kHz put
 * a step's event 4 samples late), |residual| there (in the units of the analysed signal, the
 * band-limited first difference: proportional to the click's height, about 0.36 of a step's
 * height at 48 kHz; compare sizes within this detector, not against a step's height) and the
 * score. LEVEL-RELATIVE by construction: every stage is linear or a ratio, and the floor follows
 * the peak, so a render scaled by any gain gives the same events (the check's LEVEL rows judge it
 * at 0.01, 1 and 4).
 *
 * WHAT IT DOES NOT JUDGE. The samples before the pitch stage's first full block (0.12 s at
 * 48 kHz with fmin 24 Hz: the short-term window, the pitch window, one block that crossfades in,
 * and the short-term window again over the pitch residual; warmup() returns the number), where
 * the predictors have no history, and the last `guard` samples. A render that starts with its
 * note-on therefore has its onset unjudged; put a held tone or silence before an event you need
 * judged.
 *
 * WHAT IT CANNOT TELL APART. It finds samples the preceding samples do not predict. A tone made
 * of such samples reads as events by itself, and a planted step no larger than the tone's own
 * cannot be told from them by any per-sample test: a crush blade's steps, a sample-and-hold
 * noise, a step the engine makes at its control tick, and the resets of a chord's notes or of
 * detuned members whose sum has no single period (step 4 follows one). The check prints, per
 * patch, the score the patch reaches by itself; the research note lists the patches.
 *
 * NON-FINITE INPUT THROWS: a NaN would make every score NaN and read as "no clicks".
 *
 * CLI (repo root):
 *   node tools/click_detector.mjs <file.wav> [--k <k>] [--json]
 * reads a PCM (16/24/32-bit) or IEEE-float (32/64-bit) WAV of any channel count and prints each
 * channel's events, one per line: channel, sample, seconds, size, score. --json prints the array.
 *
 * Deterministic: no clock, no randomness.
 */
import { readFileSync, realpathSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

// RATIFIED (ADR-227; header). Seconds where the number is a time; `guard` is in samples because it
// belongs to the analysis band's impulse response, which is fixed in samples.
export const DEFAULTS = Object.freeze({
  band: 0.75, lpOrder: 8, diff: true,        // 1, 2. the analysis band; the first difference
  p: 24, window: 0.01, hop: 0.00125, eta: 1e-5,         // 3. the short-term predictor
  ltp: true, fmin: 24, ltpHop: 0.01, rMin: 0.3, octave: 0.85, taps: 16, fitLen: 0.04, freeze: 0.1,   // 4. the pitch predictor
  k: 10, local: 0.02, guard: 8, merge: 0.0015, floorDb: -60,                          // 6, 7. the score and the events
});

const samplesOf = (P, sr) => ({
  M: Math.max(2 * P.p, Math.round(P.window * sr)), hop: Math.max(1, Math.round(P.hop * sr)),
  ltpHop: Math.max(1, Math.round(P.ltpHop * sr)), fitLen: Math.round(P.fitLen * sr),
  local: Math.round(P.local * sr), merge: Math.round(P.merge * sr), Tmax: Math.round(sr / P.fmin),
});
const pow2Above = v => { let w = 1; while (w < v) w <<= 1; return w; };

/* autocorrelation LPC of x[from, to): Hann window w, white-noise correction eta, Levinson-Durbin.
   a[0] = 1; the prediction error filter is sum_j a[j] x[n - j]. */
function lpcAuto(x, from, to, p, eta, a, w, seg, r, tmp) {
  const N = to - from;
  for (let i = 0; i < N; i++) seg[i] = x[from + i] * w[i];
  for (let l = 0; l <= p; l++) { let s = 0; for (let i = l; i < N; i++) s += seg[i] * seg[i - l]; r[l] = s; }
  a.fill(0); a[0] = 1;
  if (!(r[0] > 0)) return;   // silence: no prediction, the residual is the signal (zero)
  let err = r[0] * (1 + eta);
  for (let m = 1; m <= p; m++) {
    let acc = r[m];
    for (let i = 1; i < m; i++) acc += a[i] * r[m - i];
    const k = -acc / err;
    for (let i = 0; i <= m; i++) tmp[i] = a[i];
    for (let i = 1; i < m; i++) a[i] = tmp[i] + k * tmp[m - i];
    a[m] = k;
    err *= 1 - k * k;
    if (!(err > 0)) break;
  }
}

/* stage 3: the short-term residual (header). Samples before the first fit are 0 (not judged). */
export function shortResidual(x, P, S) {
  const { p, eta } = P, { M, hop } = S, n = x.length, e = new Float64Array(n);
  const a = new Float64Array(p + 1), a0 = new Float64Array(p + 1), seg = new Float64Array(M), r = new Float64Array(p + 1), tmp = new Float64Array(p + 1);
  const w = new Float64Array(M); for (let i = 0; i < M; i++) w[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * (i + 0.5) / M);
  for (let t = M; t < n; t += hop) {
    a0.set(a);
    lpcAuto(x, t - M, t, p, eta, a, w, seg, r, tmp);
    if (t === M) a0.set(a);
    const end = Math.min(n, t + hop);
    for (let i = t; i < end; i++) {
      let s0 = 0, s1 = 0;
      for (let j = 1; j <= p; j++) { s0 += a0[j] * x[i - j]; s1 += a[j] * x[i - j]; }
      const al = (i - t + 1) / hop;
      e[i] = x[i] + (1 - al) * s0 + al * s1;
    }
  }
  return e;
}

/* in-place radix-2 complex FFT (n a power of two); inv computes the unscaled inverse */
function fft(re, im, inv) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { let t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = (inv ? 2 : -2) * Math.PI / len, wr = Math.cos(ang), wi = Math.sin(ang), h = len >> 1;
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let j = 0; j < h; j++) {
        const u = i + j, v = u + h;
        const ar = re[v] * cr - im[v] * ci, ai = re[v] * ci + im[v] * cr;
        re[v] = re[u] - ar; im[v] = im[u] - ai; re[u] += ar; im[u] += ai;
        const nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
      }
    }
  }
}

/* Gaussian elimination with partial pivoting on the augmented K x (K+1) matrix m; null if singular */
function solve(m, K) {
  for (let c = 0; c < K; c++) {
    let piv = c; for (let r = c + 1; r < K; r++) if (Math.abs(m[r][c]) > Math.abs(m[piv][c])) piv = r;
    if (!(Math.abs(m[piv][c]) > 0)) return null;
    const t = m[c]; m[c] = m[piv]; m[piv] = t;
    for (let r = 0; r < K; r++) if (r !== c) { const f = m[r][c] / m[c][c]; if (f) for (let q = c; q <= K; q++) m[r][q] -= f * m[c][q]; }
  }
  const out = new Float64Array(K); for (let j = 0; j < K; j++) out[j] = m[j][K] / m[j][j];
  return out;
}

/* stage 4's plan: per block of ltpHop samples, { T, b } (taps on lags T-taps..T+taps) or null.
   w is the short-term residual (the lag is found on it: the signal's own autocorrelation is
   dominated by its low partials and peaks at the shortest lag), x the signal (the taps). */
export function ltpPlan(w, x, P, S) {
  const n = x.length, D = P.taps, K = 2 * D + 1, { Tmax, ltpHop, fitLen } = S, Tmin = D + 1;
  const WL = pow2Above(2 * Tmax), NF = 2 * WL;
  const re = new Float64Array(NF), im = new Float64Array(NF), c = new Float64Array(WL + 1), r = new Float64Array(Tmax + 2);
  const A = Array.from({ length: K }, () => new Float64Array(K + 1));
  const plan = [];
  for (let t = 0; t < n; t += ltpHop) {
    const from = t - WL;
    if (from < S.M) { plan.push(null); continue; }
    re.fill(0); im.fill(0);
    for (let i = 0; i < WL; i++) re[i] = w[from + i];
    for (let i = 0; i < WL; i++) c[i + 1] = c[i] + re[i] * re[i];
    if (!(c[WL] > 0)) { plan.push(null); continue; }
    fft(re, im, false);
    for (let i = 0; i < NF; i++) { re[i] = re[i] * re[i] + im[i] * im[i]; im[i] = 0; }
    fft(re, im, true);
    let best = -Infinity, T = 0;
    for (let L = Tmin; L <= Tmax + 1; L++) {
      const Ea = c[WL] - c[L], Eb = c[WL - L];
      r[L] = Ea > 0 && Eb > 0 ? re[L] / NF / Math.sqrt(Ea * Eb) : 0;
    }
    for (let L = Tmin; L <= Tmax; L++) if (r[L] > best) { best = r[L]; T = L; }
    if (!(best >= P.rMin)) { plan.push(null); continue; }
    // octave errors: the shortest lag that is a local peak within `octave` of the best
    for (let L = Tmin; L <= Tmax; L++) if (r[L] >= P.octave * best && r[L] >= r[L - 1] && r[L] >= r[L + 1]) { T = L; break; }
    // Least squares over [i0, t). The covariance matrix exactly: its first row directly, then down
    // each diagonal, A[r+1][q+1] = A[r][q] + (the product that enters) - (the one that leaves).
    // An unbiased-autocorrelation (Toeplitz) shortcut was tried and cancelled a saw's reset 15
    // times worse (0.18 against 0.012): the fractional delay needs the exact normal equations.
    const o = T - D, i0 = Math.max(from + T + D, t - fitLen), i1 = t;
    for (let q = 0; q < K; q++) {
      let sa = 0, sy = 0;
      for (let i = i0; i < i1; i++) { const v = x[i - o - q]; sa += x[i - o] * v; sy += x[i] * v; }
      A[0][q] = sa; A[q][K] = sy;
    }
    for (let rr = 0; rr + 1 < K; rr++) for (let q = rr; q + 1 < K; q++)
      A[rr + 1][q + 1] = A[rr][q] + x[i0 - 1 - o - rr] * x[i0 - 1 - o - q] - x[i1 - 1 - o - rr] * x[i1 - 1 - o - q];
    for (let rr = 1; rr < K; rr++) for (let q = 0; q < rr; q++) A[rr][q] = A[q][rr];
    if (!(A[0][0] > 0)) { plan.push(null); continue; }
    for (let j = 0; j < K; j++) A[j][j] *= 1 + 1e-9;   // relative loading: a singular window (pure DC) solves
    const b = solve(A, K);
    plan.push(b ? { T, b, r: best } : null);
  }
  return plan;
}
/* stage 4's residual: x minus the plan's prediction, crossfaded block to block */
export function ltpApply(x, plan, P, S) {
  const n = x.length, out = Float64Array.from(x), D = P.taps, K = 2 * D + 1, H = S.ltpHop;
  const pred = (q, i) => { if (!q) return 0; let s = 0; const o = q.T - D; for (let j = 0; j < K; j++) s += q.b[j] * x[i - o - j]; return s; };
  for (let bi = 0; bi < plan.length; bi++) {
    const q = plan[bi], q0 = bi > 0 ? plan[bi - 1] : null;
    if (!q && !q0) continue;
    const t = bi * H, end = Math.min(n, t + H);
    for (let i = t; i < end; i++) { const al = (i - t + 1) / H; out[i] = x[i] - (1 - al) * pred(q0, i) - al * pred(q, i); }
  }
  return out;
}

/* stage 1: a causal Butterworth lowpass of even order at `band` x Nyquist, as cascaded RBJ biquads
   (bilinear: every zero at Nyquist). Causal on purpose: a zero-phase filter rings BEFORE a step,
   and the predictor, seeing the ringing, partly anticipated the step (a 0.05 step on a full-band
   saw at C2 scored 4 to 7 through a zero-phase FIR, 12 and over through this). */
export function lowpass(x, band, order) {
  const n = x.length, w0 = Math.PI * band, cs = Math.cos(w0), sn = Math.sin(w0);
  let y = Float64Array.from(x);
  for (let k = 0; k < order / 2; k++) {
    const Q = 1 / (2 * Math.cos(Math.PI * (2 * k + 1) / (2 * order))), al = sn / (2 * Q), a0 = 1 + al;
    const b0 = (1 - cs) / 2 / a0, b1 = (1 - cs) / a0, b2 = b0, a1 = -2 * cs / a0, a2 = (1 - al) / a0;
    const out = new Float64Array(n);
    let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (let i = 0; i < n; i++) { const v = b0 * y[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = y[i]; y2 = y1; y1 = v; out[i] = v; }
    y = out;
  }
  return y;
}

/* the first judged sample (header, WHAT IT DOES NOT JUDGE) */
export function warmup(sr, params) {
  const P = { ...DEFAULTS, ...params }, S = samplesOf(P, sr);
  if (!P.ltp) return S.M;
  // the first block whose plan can be fitted, plus that block itself (it crossfades in from no plan),
  // plus one short-term window and block over the pitch residual (its filter is fitted on what came before)
  return (Math.ceil((S.M + pow2Above(2 * S.Tmax)) / S.ltpHop) + 1) * S.ltpHop + S.M + S.hop;
}

/* stage 6: every sample's score (0 where not judged). Prefix sums of e^2: their cancellation error
   (about 1e-16 of the render's energy) is far under the floor's energy over a window, so a quiet
   tail after a loud start is still read. A sample's own footprint is left out of its level:
   [i - guard, i + p + guard] (its residual runs through the p-tap error filter after it) and,
   where the pitch stage is on, the same span one period later, where the pitch predictor replays it. */
function scoreOf(e, plan, P, S, start, stop, floor) {
  const n = e.length, half = S.local >> 1, G = P.guard;
  const c = new Float64Array(n + 1), score = new Float64Array(n);
  for (let i = 0; i < n; i++) c[i + 1] = c[i] + e[i] * e[i];
  const sumOver = (a, b) => (b > a ? c[b] - c[a] : 0);
  for (let i = start; i < stop; i++) {
    const lo = Math.max(start, i - half), hi = Math.min(n, i + half + 1);
    const ex = [[Math.max(lo, i - G), Math.min(hi, i + P.p + G + 1)]];
    const q = plan && plan[Math.floor(i / S.ltpHop)];
    if (q) {
      const a = Math.max(lo, i + q.T - P.taps - G), b = Math.min(hi, i + q.T + P.taps + P.p + G + 1);
      if (b > a) { if (a <= ex[0][1]) ex[0][1] = Math.max(ex[0][1], b); else ex.push([a, b]); }
    }
    let cnt = hi - lo, en = sumOver(lo, hi);
    for (const [a, b] of ex) { cnt -= b - a; en -= sumOver(a, b); }
    if (cnt <= 0) continue;
    const lvl = Math.max(Math.sqrt(Math.max(0, en) / cnt), floor);
    if (lvl > 0) score[i] = Math.abs(e[i]) / lvl;
  }
  return score;
}

/* stages 1 to 6: the residual and every sample's score (0 where not judged) */
export function scores(x, sr, params) {
  const P = { ...DEFAULTS, ...params }, S = samplesOf(P, sr), n = x.length;
  let peak = 0;
  for (let i = 0; i < n; i++) {
    if (!Number.isFinite(x[i])) throw new Error(`click_detector: a non-finite sample at ${i} (a NaN render must never read as clean)`);
    if (Math.abs(x[i]) > peak) peak = Math.abs(x[i]);
  }
  const zb = P.band >= 1 ? x : lowpass(x, P.band, P.lpOrder);   // stage 1
  const z = new Float64Array(n);   // stage 2
  if (P.diff) for (let i = 1; i < n; i++) z[i] = zb[i] - zb[i - 1]; else z.set(zb);
  const e1 = shortResidual(z, P, S);
  const start = warmup(sr, P), stop = n - P.guard;
  let zpk = 0; for (let i = 0; i < n; i++) if (Math.abs(z[i]) > zpk) zpk = Math.abs(z[i]);
  const floor = zpk * Math.pow(10, P.floorDb / 20);   // relative to the peak of what is predicted (header, 6)
  if (!P.ltp) return { e: e1, score: scoreOf(e1, null, P, S, start, stop, floor), start, stop, peak, lags: null };
  const combine = plan => {   // stage 5
    const e2 = shortResidual(ltpApply(z, plan, P, S), P, S), e = new Float64Array(n);
    for (let i = 0; i < n; i++) e[i] = Math.abs(e2[i]) < Math.abs(e1[i]) ? e2[i] : e1[i];
    return e;
  };
  let plan = ltpPlan(e1, z, P, S), e = combine(plan), score = scoreOf(e, plan, P, S, start, stop, floor);
  // THE SECOND PASS. A click inside the pitch predictor's fitting window teaches it: least squares
  // bends the taps toward the click (through the rows that hold it as a regressor), and for up to
  // fitLen + a period after it the tone's own resets were no longer cancelled and read as a train
  // of events one period apart (four, after a 0.05 step on a saw at C2). Leaving those rows out
  // was tried and failed: the train's events sit on the resets, and the fit needs the reset rows.
  // So where the first pass found a hit, every block whose fitting window holds one keeps the plan
  // of the last block before it (a click must not teach the predictor), for at most `freeze`
  // seconds, after which the block's own plan is used again (a tone may move); then the whole is
  // judged again.
  const hits = [];
  for (let i = start; i < stop; i++) if (score[i] > P.k) hits.push(i);
  if (hits.length) {
    const H = S.ltpHop, reach = S.fitLen + S.Tmax + P.taps, maxHold = Math.max(1, Math.round(P.freeze * sr / H));
    const held = plan.slice();
    let hi = 0, hold = 0;
    for (let bi = 1; bi < plan.length; bi++) {
      const t = bi * H;
      while (hi < hits.length && hits[hi] < t - reach) hi++;
      const touched = hi < hits.length && hits[hi] < t && plan[bi];
      if (touched && hold < maxHold && held[bi - 1]) { held[bi] = held[bi - 1]; hold++; }
      else if (!touched) hold = 0;
    }
    plan = held; e = combine(plan); score = scoreOf(e, plan, P, S, start, stop, floor);
  }
  return { e, score, start, stop, peak, lags: Int32Array.from(plan, q => (q ? q.T : 0)) };
}

/* stage 7: events from scores() (exported so a caller with scores in hand need not recompute).
   Hits within `merge` are one event, at its highest score. An event that falls where a stronger
   earlier event's echo falls (one pitch period later: the span its score already leaves out, the
   lag being the one in force at the earlier event) is that echo and is not reported: the pitch
   predictor replays a click one period on (measured: one echo, a tenth to a twentieth of the
   click's score, on a saw at C2). COST: a weaker second click exactly one period after a first is
   not reported. */
export function eventsOf(res, sr, params) {
  const P = { ...DEFAULTS, ...params }, S = samplesOf(P, sr), { e, score, lags } = res, out = [];
  let last = -Infinity;
  for (let i = 0; i < score.length; i++) {
    if (!(score[i] > P.k)) continue;
    const l = out[out.length - 1];
    if (l && i - last <= S.merge) { if (score[i] > l.score) { l.sample = i; l.size = Math.abs(e[i]); l.score = score[i]; } }
    else out.push({ sample: i, size: Math.abs(e[i]), score: score[i] });
    last = i;
  }
  if (!lags) return out;
  return out.filter(q => !out.some(o => {
    const T = lags[Math.floor(o.sample / S.ltpHop)], d = q.sample - o.sample;
    return T > 0 && o.score > q.score && d >= T - P.taps - P.guard && d <= T + P.taps + P.p + P.guard;
  }));
}

export function detectClicks(samples, sr, params) {
  return eventsOf(scores(samples, sr, params), sr, params);
}

/* ---------------------------------------------------------------- the CLI */
/* A WAV of any channel count: PCM 16/24/32-bit or IEEE float 32/64-bit (tools/h2_ab.mjs's reader
   takes 24-bit stereo only, which is what it writes; a test suite's renders may be anything). */
export function readWav(buf) {
  if (buf.toString('latin1', 0, 4) !== 'RIFF' || buf.toString('latin1', 8, 12) !== 'WAVE') throw new Error('click_detector: not a RIFF/WAVE file');
  let p = 12, fmt = null, data = null;
  while (p + 8 <= buf.length) {
    const id = buf.toString('latin1', p, p + 4), size = buf.readUInt32LE(p + 4);
    if (id === 'fmt ') {
      let tag = buf.readUInt16LE(p + 8);
      if (tag === 0xfffe && size >= 40) tag = buf.readUInt16LE(p + 32);   // WAVE_FORMAT_EXTENSIBLE: the subformat's tag
      fmt = { tag, ch: buf.readUInt16LE(p + 10), sr: buf.readUInt32LE(p + 12), bits: buf.readUInt16LE(p + 22) };
    } else if (id === 'data') data = buf.subarray(p + 8, Math.min(buf.length, p + 8 + size));
    p += 8 + size + (size & 1);
  }
  if (!fmt || !data) throw new Error('click_detector: a WAV with no fmt or data chunk');
  const B = fmt.bits / 8, n = Math.floor(data.length / (B * fmt.ch));
  const read = fmt.tag === 3 ? (B === 4 ? q => data.readFloatLE(q) : B === 8 ? q => data.readDoubleLE(q) : null)
    : fmt.tag === 1 ? (B === 2 || B === 3 || B === 4 ? q => data.readIntLE(q, B) / 2 ** (fmt.bits - 1) : null) : null;
  if (!read) throw new Error(`click_detector: unsupported WAV (format tag ${fmt.tag}, ${fmt.bits} bits)`);
  const chans = Array.from({ length: fmt.ch }, () => new Float64Array(n));
  for (let i = 0; i < n; i++) for (let c = 0; c < fmt.ch; c++) chans[c][i] = read((i * fmt.ch + c) * B);
  return { sr: fmt.sr, chans };
}

export function main(argv) {
  const file = argv.find(a => !a.startsWith('--') && argv[argv.indexOf(a) - 1] !== '--k');
  if (!file) { console.error('usage: node tools/click_detector.mjs <file.wav> [--k <k>] [--json]'); return 64; }
  const ki = argv.indexOf('--k'), params = ki >= 0 ? { k: Number(argv[ki + 1]) } : {};
  const { sr, chans } = readWav(readFileSync(file));
  const all = chans.map((x, c) => detectClicks(x, sr, params).map(ev => ({ channel: c, ...ev, seconds: ev.sample / sr })));
  if (argv.includes('--json')) console.log(JSON.stringify(all.flat()));
  else {
    for (const ev of all.flat()) console.log(`ch ${ev.channel}  sample ${ev.sample}  ${ev.seconds.toFixed(6)} s  size ${ev.size.toExponential(3)}  score ${ev.score.toFixed(1)}`);
    console.log(`click_detector: ${all.flat().length} event(s) in ${chans.length} channel(s), ${sr} Hz, k ${params.k ?? DEFAULTS.k}; first judged sample ${warmup(sr, params)}`);
  }
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  try { process.exitCode = main(process.argv.slice(2)); }
  catch (e) { console.error(String(e && e.message || e)); process.exitCode = 2; }
}
