/*
 * metrics.mjs — B316 P2: coherence metrics of a rendered buffer. HYPERSAW, 2026-09-27,
 * ROADMAP B316 (records PR #810, branch lead-records-122).
 *
 * THESE ARE MEASUREMENTS, NOT GATES (Layer-E, oracle discipline): nothing here passes or
 * fails a patch. The gauntlet reports them; thresholds are PROVISIONAL until a blind
 * listening pass by the human calibrates them (B316: "thresholds get calibrated by a
 * short blind listening pass"). Every metric is validated on constructed signals, each
 * with a must-read-zero and a must-read-high control, in metrics_check.mjs.
 *
 * Every function is PURE: a deterministic function of the samples (and the note / sample
 * rate it is told). No clock, no RNG. The one exception is cpuFraction(), which is a pure
 * function of TIMINGS the caller measured (gauntlet.mjs reads process.hrtime around the
 * engine's render() calls only — the clock never enters the DSP).
 *
 * METHODS, stated (so a number can be argued with):
 *   nonFinite    count of NaN/±Inf samples. The engine ends in tanh(), which maps ±Inf to
 *                ±1, so an infinite INTERNAL state can read finite here; NaN survives.
 *   dc           mean per channel over the analysis window; also |mean|/RMS.
 *   level        RMS and peak (dBFS), crest factor peak/RMS (dB), and LUFS-LIKE loudness:
 *                BS.1770 K-weighting (its 48 kHz biquads), mean square summed over
 *                channels, -0.691 + 10·log10 — UNGATED, over the window (not a meter).
 *   silence      RMS below -90 dBFS.
 *   clicks       the HF residual (second difference) in 512-sample frames (hop 256); a
 *                click is a frame whose residual energy is > 20 dB above the MEDIAN frame
 *                of the non-silent frames. Periodic jumps (a saw, a hard sync edge) land in
 *                every frame and do not count; a one-off discontinuity does. Reported as
 *                the count and the worst frame's excess (dB).
 *   spectrum     Welch power spectrum of the mono sum: Blackman-Harris 4-term window
 *                (side lobes -92 dB, so a sine is ONE peak), N 8192, hop N/4.
 *   flatness     spectral flatness (geometric/arithmetic mean of power) over 50 Hz..16 kHz:
 *                ~1 for white noise, ~0 for a tone.
 *   root         harmonic-sum salience S(f) = Σ_{h≤10} 0.84^(h-1)·A(h·f), A the peak
 *                amplitude within ±30 cents; candidates every semitone from the played
 *                note (±4 octaves, kept within 40 Hz..2 kHz), the note first so a tie keeps
 *                it. rootPresence = S(note)/max S; rootInterval = the best
 *                candidate's distance from the note in semitones (0 = the note wins).
 *   roughness    Sethares (1993) dissonance over the spectral peaks (local maxima within
 *                60 dB of the largest, top 48): Σ_{i<j} a_i a_j (e^{-3.51 s Δf} - e^{-5.75 s Δf}),
 *                s = 0.24/(0.0207·min f + 18.96), normalised by Σ a_i² so it is level-free.
 *                A single sine is exactly 0. Harmonic tones have some intrinsic roughness
 *                (dense upper partials); compare against a plain saw's value, not zero.
 *   aliasing     against a REFERENCE render of the same patch at 4x the oversampling
 *                (the engine's own decimator, so best available, not alias-free): power
 *                in bins 20 Hz..20 kHz where the test exceeds the reference's local max
 *                (±3 bins) by more than 10 dB, as a fraction of the test's power (dB).
 *                The reference's own fold-back sits ≥ ~30 dB under the test's for a 4th-
 *                order decimator at 0.45·sr; the 10 dB margin and ±3 bins absorb Welch
 *                variance and the decimators' small in-band magnitude difference.
 *   cpuFraction  median over chunks of (render ns)/(chunk duration): real-time fraction.
 */

/* ---------------------------------------------------------------- basics */
export function mono(L, R) { const n = L.length, m = new Float64Array(n); for (let i = 0; i < n; i++) m[i] = 0.5 * (L[i] + R[i]); return m; }
export const db = x => (x > 0 ? 10 * Math.log10(x) : -Infinity);
const clampDb = x => (x === -Infinity ? -200 : x);

export function nonFinite(...chs) { let c = 0; for (const x of chs) for (let i = 0; i < x.length; i++) if (!Number.isFinite(x[i])) c++; return c; }
function finiteView(x) { const y = new Float64Array(x.length); for (let i = 0; i < x.length; i++) y[i] = Number.isFinite(x[i]) ? x[i] : 0; return y; }

export function dc(L, R) {
  const m = x => { let s = 0; for (let i = 0; i < x.length; i++) s += x[i]; return s / x.length; };
  const a = m(L), b = m(R), r = Math.max(rms(L), rms(R), 1e-12);
  return { dc: Math.max(Math.abs(a), Math.abs(b)), dcL: a, dcR: b, dcRatio: Math.max(Math.abs(a), Math.abs(b)) / r };
}
export function rms(x) { let s = 0; for (let i = 0; i < x.length; i++) s += x[i] * x[i]; return Math.sqrt(s / x.length); }
export function peak(x) { let p = 0; for (let i = 0; i < x.length; i++) { const a = Math.abs(x[i]); if (a > p) p = a; } return p; }

/* BS.1770 K-weighting, the standard's 48 kHz coefficients (other rates: not supported here) */
const K1 = { b: [1.53512485958697, -2.69169618940638, 1.19839281085285], a: [-1.69065929318241, 0.73248077421585] };
const K2 = { b: [1.0, -2.0, 1.0], a: [-1.99004745483398, 0.99007225036621] };
function biquad(x, f) {
  const y = new Float64Array(x.length); let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (let i = 0; i < x.length; i++) { const v = f.b[0] * x[i] + f.b[1] * x1 + f.b[2] * x2 - f.a[0] * y1 - f.a[1] * y2; x2 = x1; x1 = x[i]; y2 = y1; y1 = v; y[i] = v; }
  return y;
}
export function level(L, R, sr) {
  const r = Math.max(rms(L), rms(R)), p = Math.max(peak(L), peak(R));
  let lufs = null;
  if (sr === 48000) {
    let ms = 0; for (const x of [L, R]) { const y = biquad(biquad(x, K1), K2); ms += rms(y) ** 2; }
    lufs = clampDb(-0.691 + 10 * Math.log10(ms));
  }
  return { rmsDb: clampDb(20 * Math.log10(r)), peakDb: clampDb(20 * Math.log10(p)), crestDb: r > 0 ? 20 * Math.log10(p / r) : 0, lufs };
}
export function silence(L, R) { const r = Math.max(rms(L), rms(R)); return { silent: !(r > Math.pow(10, -90 / 20)), rmsDb: clampDb(20 * Math.log10(r)) }; }

export function clicks(x) {
  const F = 512, H = 256, e = [];
  for (let s = 2; s + F <= x.length; s += H) {
    let acc = 0; for (let i = s; i < s + F; i++) { const d = x[i] - 2 * x[i - 1] + x[i - 2]; acc += d * d; }
    e.push(acc / F);
  }
  const live = e.filter(v => v > 1e-14).sort((a, b) => a - b);
  if (live.length < 3) return { clicks: 0, worstDb: 0 };
  const med = live[live.length >> 1];
  let n = 0, worst = 0;
  for (const v of e) { const r = v / med; if (r > 100) n++; if (r > worst) worst = r; }
  return { clicks: n, worstDb: 10 * Math.log10(Math.max(worst, 1e-12)) };
}

/* ---------------------------------------------------------------- spectrum */
function fft(re, im) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) { let bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; } }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = -2 * Math.PI / len, wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let j = 0; j < len / 2; j++) {
        const a = i + j, b = a + len / 2, tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
        re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
        const t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
      }
    }
  }
}
const WIN = {};
function bh(n) { if (WIN[n]) return WIN[n]; const w = new Float64Array(n); for (let i = 0; i < n; i++) { const t = 2 * Math.PI * i / (n - 1); w[i] = 0.35875 - 0.48829 * Math.cos(t) + 0.14128 * Math.cos(2 * t) - 0.01168 * Math.cos(3 * t); } return (WIN[n] = w); }
/* Welch power spectrum (window-power normalised, so a sine of amplitude a reads a²/2 summed over its lobe) */
export function spectrum(x, sr, N) {
  N = N || 8192;
  x = finiteView(x);
  const w = bh(N), hop = N >> 2, P = new Float64Array(N / 2 + 1);
  let U = 0; for (let i = 0; i < N; i++) U += w[i] * w[i];
  let frames = 0;
  const starts = [];
  for (let s = 0; s + N <= x.length; s += hop) starts.push(s);
  if (!starts.length) starts.push(0);
  for (const s of starts) {
    const re = new Float64Array(N), im = new Float64Array(N);
    for (let i = 0; i < N; i++) re[i] = (x[s + i] || 0) * w[i];
    fft(re, im);
    for (let k = 0; k <= N / 2; k++) P[k] += (re[k] * re[k] + im[k] * im[k]) * (k === 0 || k === N / 2 ? 1 : 2) / (U * N);
    frames++;
  }
  for (let k = 0; k < P.length; k++) P[k] /= frames;
  return { P, binHz: sr / N, N, frames };
}
export function flatness(S, lo, hi) {
  lo = lo || 50; hi = hi || 16000;
  const a = Math.max(1, Math.ceil(lo / S.binHz)), b = Math.min(S.P.length - 1, Math.floor(hi / S.binHz));
  let lg = 0, ar = 0, n = 0;
  for (let k = a; k <= b; k++) { const p = S.P[k] + 1e-30; lg += Math.log(p); ar += p; n++; }
  return Math.exp(lg / n) / (ar / n);
}
/* amplitude at frequency f: the peak magnitude within ±cents */
function ampAt(S, f, cents) {
  const r = Math.pow(2, cents / 1200), a = Math.max(1, Math.floor(f / r / S.binHz)), b = Math.min(S.P.length - 1, Math.ceil(f * r / S.binHz));
  let m = 0; for (let k = a; k <= b; k++) if (S.P[k] > m) m = S.P[k];
  return Math.sqrt(m);
}
export function salience(S, f) { let s = 0; for (let h = 1; h <= 10; h++) { if (h * f >= S.binHz * (S.P.length - 1)) break; s += Math.pow(0.84, h - 1) * ampAt(S, h * f, 30); } return s; }
export function root(S, noteHz) {
  const cands = [];                                   // the note first, so a tie keeps the note
  for (const k of [0].concat(Array.from({ length: 96 }, (_, i) => (i < 48 ? i - 48 : i - 47)))) cands.push(noteHz * Math.pow(2, k / 12));
  let best = 0, bestF = noteHz;
  for (const f of cands) { if (f < 40 || f > 2000) continue; const s = salience(S, f); if (s > best) { best = s; bestF = f; } }
  const sN = salience(S, noteHz);
  return { rootPresence: best > 0 ? sN / best : 0, rootInterval: Math.round(12 * Math.log2(bestF / noteHz) * 100) / 100, bestHz: bestF };
}
export function peaks(S, maxN, floorDb) {
  maxN = maxN || 48; floorDb = floorDb === undefined ? 60 : floorDb;
  let top = 0; for (let k = 1; k < S.P.length; k++) if (S.P[k] > top) top = S.P[k];
  const thr = top * Math.pow(10, -floorDb / 10), pk = [];
  for (let k = 2; k < S.P.length - 2; k++) {
    const p = S.P[k];
    if (p > thr && p >= S.P[k - 1] && p > S.P[k + 1] && p >= S.P[k - 2] && p > S.P[k + 2]) {
      /* parabolic interpolation on log power for the frequency */
      const a = Math.log(S.P[k - 1] + 1e-300), b = Math.log(p), c = Math.log(S.P[k + 1] + 1e-300), d = 0.5 * (a - c) / (a - 2 * b + c || 1e-12);
      let e = 0; for (let j = k - 3; j <= k + 3; j++) if (j > 0 && j < S.P.length) e += S.P[j];   // the lobe's power
      pk.push({ f: (k + d) * S.binHz, a: Math.sqrt(2 * e) });
    }
  }
  return pk.sort((x, y) => y.a - x.a).slice(0, maxN);
}
export function roughness(S) {
  const pk = peaks(S);
  let D = 0, E = 0;
  for (let i = 0; i < pk.length; i++) {
    E += pk[i].a * pk[i].a;
    for (let j = i + 1; j < pk.length; j++) {
      const f1 = Math.min(pk[i].f, pk[j].f), df = Math.abs(pk[i].f - pk[j].f), s = 0.24 / (0.0207 * f1 + 18.96);
      D += pk[i].a * pk[j].a * (Math.exp(-3.51 * s * df) - Math.exp(-5.75 * s * df));
    }
  }
  return { roughness: E > 0 ? D / E : 0, peaks: pk.length };
}
export function aliasing(Stest, Sref) {
  if (Stest.binHz !== Sref.binHz) throw new Error('aliasing: spectra need the same bin width');
  const a = Math.ceil(20 / Stest.binHz), b = Math.min(Stest.P.length - 1, Math.floor(20000 / Stest.binHz));
  let tot = 0, ex = 0;
  for (let k = a; k <= b; k++) {
    let m = 0; for (let j = Math.max(0, k - 3); j <= Math.min(Sref.P.length - 1, k + 3); j++) if (Sref.P[j] > m) m = Sref.P[j];
    tot += Stest.P[k];
    if (Stest.P[k] > 10 * m) ex += Stest.P[k];
  }
  const frac = tot > 0 ? ex / tot : 0;
  return { aliasFraction: frac, aliasDb: frac > 0 ? Math.max(-120, 10 * Math.log10(frac)) : -120 };
}
/* timing: [[samples, ns], …] from the caller; median real-time fraction over chunks */
export function cpuFraction(timing, sr) {
  const f = timing.map(([n, ns]) => (ns / 1e9) / (n / sr)).sort((a, b) => a - b);
  if (!f.length) return null;
  const m = f.length >> 1;
  return f.length % 2 ? f[m] : 0.5 * (f[m - 1] + f[m]);
}

/* ---------------------------------------------------------------- the bundle */
/* everything but aliasing and CPU (which need a second render / timings): one call per
   analysis window. `noteHz` is the played note's frequency. */
export function analyse(L, R, sr, noteHz) {
  const m = mono(L, R), fin = finiteView(m);
  const S = spectrum(fin, sr);
  const lv = level(finiteView(L), finiteView(R), sr), si = silence(finiteView(L), finiteView(R));
  return Object.assign({ nonFinite: nonFinite(L, R) }, dc(finiteView(L), finiteView(R)), lv, si, clicks(fin),
    { flatness: si.silent ? null : flatness(S) }, si.silent ? { rootPresence: null, rootInterval: null } : root(S, noteHz),
    si.silent ? { roughness: null } : { roughness: roughness(S).roughness }, { _S: S });
}
