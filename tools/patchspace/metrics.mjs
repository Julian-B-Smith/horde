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
 *                (side lobes -92 dB, so a sine is ONE peak), N 8192, hop N/4. The
 *                per-frame powers are kept too (`F`): aliasing and the noise floor are
 *                taken frame by frame (B345, below), never from the whole-window average.
 *   flatness     spectral flatness (geometric/arithmetic mean of power) over 50 Hz..16 kHz:
 *                ~1 for white noise, ~0 for a tone. KEPT, but B345 found it does not track
 *                what the human calls noisy (it is a ratio of means over the whole band, so a
 *                tone with noise 20 dB under it reads ~0.01); noiseDb is its proposed
 *                replacement.
 *   noiseDb      APERIODIC POWER (B345): the share of the power in 50 Hz..16 kHz that is a
 *                continuous floor rather than spectral lines, in dB (0 = all noise). Per
 *                frame, the floor under every bin is noiseFloor(): the 10th percentile of the
 *                frame's power over ±32 bins (±188 Hz), divided by -ln(0.9) so that for a
 *                noise region (bins exponentially distributed) it is the region's MEAN, and
 *                below any line (a tone's lobes cover < 90% of the window's bins down to A1).
 *                noiseDb = 10·log10(Σ floor / Σ power) over the window's frames. Validated on
 *                constructed signals: a tone plus white noise at a known in-band share reads
 *                that share within ~2 dB (A3/E5; ~+4 dB at A1); a pure tone, a beating chord
 *                and an inharmonic cloud of sines read below -60 dB (lines, not noise). Its
 *                stated limit: partials closer than the analysis resolution (a 7-voice ±25-
 *                cent supersaw's upper harmonics at A3) fill the valleys and read as floor
 *                (about -27 dB), so a dense swarm reads part-noisy.
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
 *                in bins 20 Hz..20 kHz where the test exceeds the reference's local level by
 *                more than 10 dB, as a fraction of the test's power (dB). The reference's own
 *                fold-back sits ≥ ~30 dB under the test's for a 4th-order decimator at
 *                0.45·sr; the 10 dB margin absorbs the decimators' small in-band magnitude
 *                difference. B345 (2026-09-28) fixed two defects, and the method is now:
 *                (1) THRESHOLD, THEN AGGREGATE. The comparison is made on UNITS of two
 *                consecutive frames (the Welch average the gauntlet's own 0.25 s window
 *                holds: 12000 samples are exactly 2 frames), sliding one frame at a time; the
 *                window's value is Σ counted power / Σ power over all units. Before B345 the
 *                whole window was averaged FIRST and compared once, so over a long window a
 *                moving partial smeared into an envelope that hid the aliases between the
 *                partials: a naive saw gliding 2 semitones/s read -30.7 dB over 0.25 s and
 *                -40.7 over 1.45 s, and the engine's broad#511 read -120 over its 1.45 s hold
 *                while every 0.25 s part read -30 to -35 (B342(1); the B340 vibrato test
 *                missed it because a vibrato inside one window smears both spans alike). Now
 *                a steady or steadily moving signal reads the same at any window length
 *                (metrics_check M11), and a 2-frame window reads exactly as before.
 *                (2) THE REFERENCE'S LOCAL LEVEL IS max(its ±3-bin max, its noise floor):
 *                noiseFloor(), the same estimator as noiseDb. Before B345 it was the ±3-bin
 *                max alone, and on noise-like content a chance dip in the reference's
 *                spectrum let the test's own chance peak count as "folded": two independent
 *                renders of the SAME band-limited noise (no folding by construction) read
 *                -23 to -31 dB in a third to a half of their 0.25 s windows (B342(2)'s
 *                hypothesis: the 1x and 4x renders of a noise-like patch are different
 *                realisations). broad#828, which raised it, is NOT this case: a same-os,
 *                other-seed reference reads -120 on it, and its 1x render carries a band near
 *                0.2 x the internal rate (9.6 kHz at os 1, 19.3 kHz at os 2), strongest in the
 *                attack: real content the 4x render lacks, so it still reads about -5 dB. The
 *                floor fills the dips; below sparse lines it sits in the valleys, so tonal
 *                aliasing is read as before. Two stated limits: noise whose LEVEL differs still
 *                reads (white noise drawn per sample at each rate, the 1x floor 6 dB higher in
 *                band, reads about -19 dB: folded noise, which is folding but not tonal); and
 *                where the reference's partials are dense and drifting (broad#511's upper
 *                octave, a 1.6 s attack under 88 cents of drift) the floor rises toward the
 *                partials and weak aliases between them stop counting: its E5 windows read
 *                -40 to -46 dB or -120, where they read -30 to -35 before (its worst over the
 *                v2 program still reads -23 dB, on the sweep).
 *                (3) A unit whose test power in band is under -90 dBFS is silent and is not
 *                measured (edge#145's sweep read 0 dB when its tail decayed to subnormal
 *                samples against a reference that had reached exact zero).
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
/* Welch power spectrum (window-power normalised, so a sine of amplitude a reads a²/2 summed over
   its lobe). `F` keeps each frame's power (P is their mean, accumulated in the same order as
   before B345, so P is bit-identical to the pre-B345 spectrum). */
export function spectrum(x, sr, N) {
  N = N || 8192;
  x = finiteView(x);
  const w = bh(N), hop = N >> 2, P = new Float64Array(N / 2 + 1), F = [];
  let U = 0; for (let i = 0; i < N; i++) U += w[i] * w[i];
  let frames = 0;
  const starts = [];
  for (let s = 0; s + N <= x.length; s += hop) starts.push(s);
  if (!starts.length) starts.push(0);
  for (const s of starts) {
    const re = new Float64Array(N), im = new Float64Array(N), Pf = new Float64Array(N / 2 + 1);
    for (let i = 0; i < N; i++) re[i] = (x[s + i] || 0) * w[i];
    fft(re, im);
    for (let k = 0; k <= N / 2; k++) { Pf[k] = (re[k] * re[k] + im[k] * im[k]) * (k === 0 || k === N / 2 ? 1 : 2) / (U * N); P[k] += Pf[k]; }
    F.push(Pf);
    frames++;
  }
  for (let k = 0; k < P.length; k++) P[k] /= frames;
  return { P, F, binHz: sr / N, N, frames };
}
/* THE NOISE FLOOR under bins a..b of one power spectrum P (B345): the 10th percentile of P over
   ±32 bins around each block of 8 bins, divided by -ln(0.9). A noise bin's power is
   exponentially distributed, so over a noise region this is the region's MEAN power; under
   spectral lines it is the valley level between them (a tone's lobes cover < 90% of ±32 bins
   even at A1, where the harmonics are 9.4 bins apart and each lobe is 8 wide). Returned per
   bin, index k - a. Shared by noiseDb and aliasing's reference level — one estimator. */
const FLOOR_Q = 0.1, FLOOR_W = 32, FLOOR_B = 8, FLOOR_CORR = -Math.log(1 - FLOOR_Q);
export function noiseFloor(P, a, b) {
  const out = new Float64Array(b - a + 1);
  for (let c = a; c <= b; c += FLOOR_B) {
    const lo = Math.max(1, c - FLOOR_W), hi = Math.min(P.length - 1, c + FLOOR_B - 1 + FLOOR_W);
    const v = Array.from(P.subarray(lo, hi + 1)).sort((x, y) => x - y), f = v[Math.floor(FLOOR_Q * (v.length - 1))] / FLOOR_CORR;
    for (let k = c; k <= Math.min(b, c + FLOOR_B - 1); k++) out[k - a] = f;
  }
  return out;
}
/* noiseDb: the aperiodic share of the power in lo..hi, frame by frame (see the header) */
export function aperiodic(S, lo, hi) {
  lo = lo || 50; hi = hi || 16000;
  const a = Math.max(1, Math.ceil(lo / S.binHz)), b = Math.min(S.P.length - 1, Math.floor(hi / S.binHz));
  let fl = 0, tot = 0;
  for (const P of S.F) { const f = noiseFloor(P, a, b); for (let k = a; k <= b; k++) { fl += f[k - a]; tot += P[k]; } }
  const frac = tot > 0 ? Math.min(1, fl / tot) : 0;
  return { noiseFraction: frac, noiseDb: frac > 0 ? Math.max(-120, 10 * Math.log10(frac)) : -120 };
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
/* aliasing (see the header, B345 (1)-(3)): 2-frame UNITS sliding one frame, each compared on its
   own, the counted power summed over units. The two spectra must be frame-aligned: the same bin
   width and the same frame starts in time (a 4x-rate reference takes a 4x FFT, so its hop is
   the test's in seconds). */
const UNIT = 2, SILENT_POW = 1e-9;                  // -90 dBFS RMS in band: metrics' silence rule
export function aliasing(Stest, Sref) {
  if (Stest.binHz !== Sref.binHz) throw new Error('aliasing: spectra need the same bin width');
  if (Stest.F.length !== Sref.F.length) throw new Error('aliasing: spectra need the same frames (the reference must span the test\'s window)');
  const a = Math.ceil(20 / Stest.binHz), b = Math.min(Stest.P.length - 1, Math.floor(20000 / Stest.binHz));
  const U = Math.min(UNIT, Stest.F.length);
  /* the unit's mean power, summed in the order spectrum() sums P: a 2-frame window's unit IS P */
  const unit = (F, u, n) => { const P = new Float64Array(n); for (let g = u; g < u + U; g++) for (let k = 0; k < n; k++) P[k] += F[g][k]; for (let k = 0; k < n; k++) P[k] /= U; return P; };
  let tot = 0, ex = 0;
  for (let u = 0; u + U <= Stest.F.length; u++) {
    const Pt = unit(Stest.F, u, Stest.P.length), Pr = unit(Sref.F, u, Sref.P.length), fl = noiseFloor(Pr, a, b);
    let ut = 0, ue = 0;
    for (let k = a; k <= b; k++) {
      let m = fl[k - a]; for (let j = Math.max(0, k - 3); j <= Math.min(Pr.length - 1, k + 3); j++) if (Pr[j] > m) m = Pr[j];
      ut += Pt[k];
      if (Pt[k] > 10 * m) ue += Pt[k];
    }
    if (ut < SILENT_POW) continue;                  // a silent unit is not measured (B345 (3))
    tot += ut; ex += ue;
  }
  const frac = tot > 0 ? ex / tot : 0;
  return { aliasFraction: frac, aliasDb: frac > 0 ? Math.max(-120, 10 * Math.log10(frac)) : -120 };
}
/* ---------------------------------------------------------------- B346: the os-convergence estimator */
/* aliasConvergence (B346, 2026-09-29; the human: "I think we should try to build the cleanest system we
   can muster"). KEPT BESIDE aliasing() above, not replacing it, until it is proven (ROADMAP B346 (a)).
   WHAT IT ASKS. Folding is what a render at os N has that the same patch at finer rates does not, AND
   what the finer renders' own content above N's Nyquist predicts. Discretisation (rate-dependent
   dynamics: a loop whose delay is one internal sample, a limit cycle locked to the internal rate) also
   makes N differ from finer rates, but no finer render's content predicts it. So, on five renders of one
   patch at os N, 2N, 4N, 8N, 16N (every one at the output rate, frame-aligned) plus the PRE-DECIMATION
   stream of the 8N render (the caller captures it; see alias_sources.mjs):
     (1) EXCESS at N against TWO references, 4N and 8N (not 2N: 2N's own aliases would mask N's;
         broad#511 E5 read about 7 dB low against {2N, 4N}). Units as in aliasing() (2 frames, sliding
         one), the band 20 Hz..20 kHz, and REGIONS of 33 bins (193 Hz). Every spectrum is first divided
         by its own render's decimation filter (gains[], from the caller: the engine's biquads differ with
         os, and os 1 has none, so an os-1 test is up to 1.9 dB brighter at 20 kHz by construction). Per
         region, the two references decide how to compare:
           SAME REALISATION (at least 80% of the references' power in bins where they agree within 10%):
             bin for bin (the renders are aligned, so no +/-3-bin max), the power the test has beyond the
             larger reference wherever that bin's power at least DOUBLED; or the per-bin rule below if that
             counts more. Folded power ADDS power even where it cannot be resolved from a partial: under a
             30-cent vibrato at 5 Hz a partial at 13 kHz sweeps +/-230 Hz inside one 0.17 s frame, so at A3
             no 10x test can separate an alias from the smeared partials (a naive saw at A3 read -120 with
             B345's aliasing() under that vibrato, -44 with the 10x rule alone, -27 here; steady it reads
             -24: metrics_check X7). The doubling (3 dB) is what keeps a rate-dependent LEVEL out: a first
             try summed the region's whole power difference over a 10% margin, and a sound 1.5 dB louder at
             os N than at its finer rates read as -5.3 dB of folding (metrics_check X8).
           NOT THE SAME REALISATION (noise, chaos): per bin, the test over 10x the +/-3-bin max of BOTH
             references. NO NOISE FLOOR: the floor was B345's cure for a chance dip in ONE reference letting
             a chance peak count, and its cost was hiding weak aliases between dense partials (broad#511's
             E5 read -120 or -40 to -46 where its aliases sit near -32). Two independent references cure
             the same thing at no such cost: a chance dip must occur in both at once (metrics_check X3:
             independent noise renders read -120).
     (2) CONVERGENCE: the same excess one octave up (2N against {8N, 16N}); convDb = excess(N) -
         excess(2N). Folding shrinks as N grows (a naive saw: about 5-11 dB per doubling on this count).
     (3) THE SOURCE TEST: fold the 8N render's pre-decimation spectrum onto the output band as the
         render at N would, bin for bin (an image at m*sr +/- f lands on output bin f; bin widths match
         because every FFT spans the same 0.17 s), weighted by N's decimation filter where the image
         reaches it (1 at os 1, which has no filter). Counted power is EXPLAINED up to 4x (6 dB) its
         prediction, per bin or per region as it was counted. foldDb is the explained share, dynDb the rest.
   CLASS: 'clean' if excess(N) <= -60 dB; else 'folding' if the excess CONVERGES (convDb >= 3 dB) AND
   at least half of it is explained by the source test; else 'dynamics'. BOTH are required, because
   each alone is fooled by one case, measured: convergence alone calls a loop instability that
   vanishes at 2N folding (sine feedback FM at beta 0.6 with the engine's loop: a limit cycle near a
   third of the internal rate at os 1, gone at os 2; metrics_check X5); the source test alone calls
   broad#828's rate-locked limit cycle folding (82% "explained": the 8N render's own broadband
   content above N's Nyquist sits at the images by coincidence), which convergence rejects (its
   excess falls 1.6 dB per doubling).
   WHAT IT CANNOT SEE, STATED: anything every os shares. The engine's final tanh runs at the OUTPUT rate
   and its decimator is a 4th-order Butterworth at 0.45 sr followed by picking one sample in os; both
   make the same aliases at every N, so neither this estimator nor aliasing() above can see them (the
   harness measures them against an ideal decimator and an oversampled tanh: alias_sources.mjs
   outputStage). And the source test is only as sharp as the fine stream is sparse: where that stream is
   broadband (a noise source runs on above N's Nyquist), it "explains" any excess at N, and the class
   then rests on convergence (metrics_check X4 reads its planted tone as folding for that reason). At
   A1 (harmonics 9.4 bins apart, lobes 8 wide) the per-bin rule is blind, and the region power
   difference is what reads a naive saw's -32 dB (B345's aliasing() reads -120; metrics_check X2i). */
export const CONV_CLEAN_DB = -60, CONV_EXPLAIN = 4, CONV_REGION = 33, CONV_AGREE = 0.1, CONV_SAME = 0.8, CONV_BIN = 2;
const bandOf = S => [Math.ceil(20 / S.binHz), Math.min(S.P.length - 1, Math.floor(20000 / S.binHz))];
function unitMean(F, u, n, U) { const P = new Float64Array(n); for (let g = u; g < u + U; g++) for (let k = 0; k < n; k++) P[k] += F[g][k]; for (let k = 0; k < n; k++) P[k] /= U; return P; }
const toDb = f => (f > 0 ? Math.max(-120, 10 * Math.log10(f)) : -120);
/* the excess of Stest over TWO references (see (1)). `gains` (optional) = [test, ref1, ref2] power-gain
   functions of Hz (each render's decimation filter; absent = 1); `pred` (optional, per unit: (u) ->
   Float64Array of predicted folded power per output bin) splits the counted power into explained and
   not. A single reference may be given twice: the region is then always "the same realisation" (right
   for deterministic transforms of one stream, as alias_sources.mjs outputStage compares). */
export function excessJoint(Stest, refs, pred, gains) {
  if (refs.length !== 2) throw new Error('excessJoint: two references (give one twice for a deterministic transform)');
  for (const R of refs) {
    if (R.binHz !== Stest.binHz) throw new Error('excessJoint: spectra need the same bin width');
    if (R.F.length !== Stest.F.length) throw new Error('excessJoint: spectra need the same frames');
  }
  const [a, b] = bandOf(Stest), n = Stest.P.length, U = Math.min(UNIT, Stest.F.length);
  const inv = g => { const c = new Float64Array(n); for (let k = 0; k < n; k++) c[k] = g ? 1 / Math.max(g(k * Stest.binHz), 1e-6) : 1; return c; };
  const [cT, c1, c2] = (gains || [null, null, null]).map(inv);
  let tot = 0, ex = 0, exp = 0, same = 0, regions = 0;
  for (let u = 0; u + U <= Stest.F.length; u++) {
    const Pt = unitMean(Stest.F, u, n, U), P1 = unitMean(refs[0].F, u, n, U), P2 = refs[1] === refs[0] ? P1 : unitMean(refs[1].F, u, n, U), Pp = pred ? pred(u) : null;
    let ut = 0; for (let k = a; k <= b; k++) ut += Pt[k];
    if (ut < SILENT_POW) continue;                  // a silent unit is not measured (aliasing()'s rule)
    let ue = 0, uex = 0;
    for (let c = a; c <= b; c += CONV_REGION) {
      const hi = Math.min(b, c + CONV_REGION - 1);
      let sT = 0, sR = 0, s1 = 0, s2 = 0, agree = 0, bins = 0, binsEx = 0, pr = 0, dPow = 0;
      for (let k = c; k <= hi; k++) {
        const x1 = P1[k] * c1[k], x2 = P2[k] * c2[k], xt = Pt[k] * cT[k], xm = Math.max(x1, x2);
        if (Math.abs(x1 - x2) <= CONV_AGREE * (x1 + x2)) agree += x1 + x2;
        if (xt > CONV_BIN * xm) dPow += (xt - xm) / cT[k];      // this bin at least doubled: power the references lack
        sT += xt; sR += Pt[k]; s1 += x1; s2 += x2; if (Pp) pr += Pp[k];
        let m = 0; for (const P of [P1, P2]) for (let j = Math.max(0, k - 3); j <= Math.min(n - 1, k + 3); j++) if (P[j] > m) m = P[j];
        if (Pt[k] > 10 * m) { bins += Pt[k]; if (Pp) binsEx += Math.min(Pt[k], CONV_EXPLAIN * Pp[k]); }
      }
      regions++;
      let cnt = bins, cex = binsEx;
      if (agree >= CONV_SAME * (s1 + s2)) {
        same++;
        if (dPow > cnt) { cnt = dPow; cex = Math.min(dPow, CONV_EXPLAIN * pr); }
      }
      ue += cnt; uex += cex;
    }
    tot += ut; ex += ue; exp += uex;
  }
  const f = tot > 0 ? ex / tot : 0, fe = tot > 0 ? exp / tot : 0;
  return { fraction: f, explained: fe, db: toDb(f), explainedDb: toDb(fe), unexplainedDb: toDb(Math.max(0, f - fe)), sameShare: regions ? same / regions : null };
}
/* the predicted folded power at os N, per output bin, from a FINE pre-decimation spectrum `Sint`
   (rate sr*kFine, an FFT of 8192*kFine points so its bins are the output spectra's width, frame-aligned):
   every image m*sr +/- f of output bin f below the fine Nyquist lands on f, weighted by N's decimation
   filter at the frequency the image reaches inside the N render (fold into [0, sr*N/2]). The result is
   per unit, as excessJoint expects. */
export function foldPrediction(Sint, sr, osN, nOut, decimGain2) {
  const per = Math.round(sr / Sint.binHz), RN = sr * osN, n = Sint.P.length, g2 = decimGain2 || (() => 1), U = Math.min(UNIT, Sint.F.length);
  if (Math.abs(per * Sint.binHz - sr) > 1e-6 * sr) throw new Error('foldPrediction: the fine spectrum\'s bin width must divide the output rate');
  const W = new Float64Array(n);                    // N's decimation filter at the frequency each fine bin reaches inside the N render
  for (let kk = 0; kk < n; kk++) { const fs = kk * Sint.binHz, fi = Math.abs(fs - RN * Math.round(fs / RN)); W[kk] = osN > 1 ? g2(fi) : 1; }
  return u => {
    const P = unitMean(Sint.F, u, n, U), out = new Float64Array(nOut);
    for (let k = 1; k < nOut; k++) {
      let s = 0;
      for (let m = 1; m * per - k < n; m++) { const lo = m * per - k, hi = m * per + k; s += P[lo] * W[lo]; if (hi < n) s += P[hi] * W[hi]; }
      out[k] = s;
    }
    return out;
  };
}
/* the estimator. S = { N, N2, N4, N8, N16 } output-rate spectra of one patch at os N..16N (same window);
   Sint = the 8N render's pre-decimation spectrum (optional: without it every excess is unexplained and
   the class is decided by convergence alone, stated in `basis`); osN the test's oversampling; sr the
   output rate; gainOf(os) (optional) -> the power gain of the decimation filter of a render at that os,
   a function of Hz (the engine's; absent = 1 everywhere, as for renders that share one decimator). */
export function aliasConvergence(S, Sint, osN, sr, gainOf) {
  const g = os => (gainOf ? gainOf(os) : null);
  const pred = Sint ? foldPrediction(Sint, sr, osN, S.N.P.length, g(osN)) : null;
  const e1 = excessJoint(S.N, [S.N4, S.N8], pred, [g(osN), g(4 * osN), g(8 * osN)]), e2 = excessJoint(S.N2, [S.N8, S.N16], null, [g(2 * osN), g(8 * osN), g(16 * osN)]);
  const convDb = e1.db - e2.db;
  let cls, basis;
  if (e1.db <= CONV_CLEAN_DB) { cls = 'clean'; basis = 'excess at or under ' + CONV_CLEAN_DB + ' dB'; }
  else if (pred) { cls = convDb >= 3 && e1.explained >= 0.5 * e1.fraction ? 'folding' : 'dynamics'; basis = 'convergence and the source test'; }
  else { cls = convDb >= 3 ? 'folding' : 'dynamics'; basis = 'convergence only (no pre-decimation spectrum)'; }
  return { excessDb: e1.db, foldDb: e1.explainedDb, dynDb: e1.unexplainedDb, excess2Db: e2.db, convDb, cls, basis,
    explainedShare: e1.fraction > 0 ? e1.explained / e1.fraction : null, sameShare: e1.sameShare };
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
    { flatness: si.silent ? null : flatness(S), noiseDb: si.silent ? null : aperiodic(S).noiseDb }, si.silent ? { rootPresence: null, rootInterval: null } : root(S, noteHz),
    si.silent ? { roughness: null } : { roughness: roughness(S).roughness }, { _S: S });
}
