/*
 * engine2_differential.mjs -- the differential report of the new engine against the legacy one, and the
 * capped listening batch it ranks (ROADMAP B523; ADR-247 audit P8; spec 10.6 item 3).
 *
 * WIRED: ./verify fast (--selftest only: synthetic WAV pairs, no engine, no real renders, a few seconds).
 *        The real run is by hand, by the lead, at the bank stage; it is REPORTED, NEVER A GATE, and it
 *        runs in neither clean-room tree (docs/port/differential-report.md is the how-to).
 *
 *   node tools/engine2_differential.mjs --legacy <dir> --new <dir> --out <dir> [--batch 20] [--seed 1]
 *   node tools/engine2_differential.mjs --selftest
 *
 * WHAT IT COMPARES. --legacy is the unzipped release asset of engine2/data/reference-renders (the current
 * engine's renders: 24-bit stereo, unnormalised, manifest.json beside them); --new holds the new engine's
 * renders of the same (preset, phrase) pairs under the SAME FILE NAMES (16-bit, 24-bit or 32-bit float PCM;
 * the same rate). Only the 83 BANK presets are judged: the 12 envelope presets are skipped and listed as
 * such. A pair present on one side only is reported by name, never dropped silently; a pair that cannot be
 * measured (a render too quiet or too short for the loudness gates, a rate or channel mismatch, a NaN) is
 * listed with its reason and is neither ranked nor batched.
 *
 * FOUR MEASURES PER PAIR (new minus legacy where a sign exists):
 *   1. LOUDNESS DELTA, LU: ITU-R BS.1770-4 integrated loudness (K-weighting; 400 ms blocks, 75 % overlap;
 *      the absolute gate at -70 LUFS and the relative gate 10 LU under the gated mean). The K-filters'
 *      coefficients are computed at the render's own rate from the standard's analogue prototype (at 48 kHz
 *      they are the standard's table). A render with no block over the gates reads "too quiet", not a number.
 *   2. THIRD-OCTAVE SPECTRAL DISTANCE: the long-term power (a Welch average, Hann, 16384 points) in the 29
 *      IEC 61260 base-10 bands from 25 Hz to 16 kHz, each in dB RELATIVE TO ITS OWN RENDER'S K-WEIGHTED
 *      LOUDNESS, so a level difference cancels and this measures timbre. Reported: the mean absolute band
 *      difference (dB) and the worst band with its centre. A band is floored at -90 dB re the loudness: below
 *      that is quantisation noise (a 16-bit file's floor is higher than a 24-bit one's) and would otherwise
 *      read as a spectral difference between two silences.
 *   3. ENVELOPE CORRELATION: the RMS envelope in 10 ms frames, in dB with a -90 dB floor, over the common
 *      length; the new render is first scaled to the legacy loudness so a level offset cannot move the
 *      floor. Pearson r (1 = the same shape) and the largest frame difference in dB with its time. A flat
 *      envelope against a moving one reads 0; two flat ones read 1 (and the frame difference still shows
 *      a level offset).
 *   4. CLICK-COUNT DELTA: events of tools/click_detector.mjs (imported, its ratified defaults, ADR-227),
 *      per channel and summed, new minus legacy. CAVEAT: that detector does not judge the first warmup()
 *      samples (0.12 s) and every phrase starts with a note-on at sample 0, so a click AT THE ONSET of
 *      either engine is not in this count.
 *
 * THE DIFFERENCE SCORE (the rule; the report header repeats it). For each measure m of the four, over all
 * measurable pairs, a pair's term is the RANK-NORMALISED position of its value: (the number of pairs with a
 * strictly smaller value) / (pairs - 1), so 0 for the least different and 1 for the most. The values are
 *     |loudness delta|,  spectral mean absolute difference,  1 - envelope correlation,  max(0, click delta).
 * score = the sum of the four terms (0 to 4). WHY THIS RULE: the measures have four different units and
 * distributions, and any weighting between them would be a tuned number nobody has measured; ranks need
 * none, and one wild outlier cannot swamp the other three. Equal weights are a choice, not a finding. WHY
 * max(0, click delta): the question is whether the new engine clicks where the old did not; if it clicks
 * LESS that is not a difference to listen for. Strict-less counting gives an identical pair exactly 0 and
 * puts every non-positive click delta at 0 (a tie is not a rank). Ties in the score break by preset id,
 * then phrase, so the order is deterministic. The score orders a listening batch; it is not a verdict.
 *
 * THE BATCH. The top --batch (20) pairs by score, AT MOST ONE PER PRESET (each preset's highest-scoring
 * phrase), ranked order. Per pair NN: <out>/batch/NN-a.wav and NN-b.wav, the legacy render and the new
 * render scaled to the legacy integrated loudness, BOTH written by the same 24-bit PCM writer so the format
 * does not say which is which. The a/b draw is a seeded mulberry32 (--seed, default 1), one draw per pair
 * in rank order; <out>/batch/key.json says which is which and is the human's answer sheet, so it is the
 * only place the assignment is written. CLIP-SAFE: if the scaled new render would pass full scale, BOTH
 * files of the pair are scaled down by the same factor (the loudness difference stays 0) and the key and
 * the report say so. Files in the batch have their original lengths: a length difference between engines
 * is real and audible.
 *
 * OUTPUT. <out>/report.json (every pair, every measure, the terms, score and rank), <out>/report.md (the
 * rule, the batch, the full ranked table, the missing, skipped, unmeasured), <out>/batch/. The tool refuses
 * an --out inside this repository unless it is under local/ (gitignored): it never writes a tracked path.
 *
 * THE SELF-TEST (--selftest) builds synthetic WAV pairs under os.tmpdir() and checks the measures against
 * values worked out independently (the standard's calibration, the K-filters' analytic response, a known
 * tilt) and the ranking and batch end to end. Then each MUST-FAIL CONTROL swaps one deliberately broken
 * measure into the same battery (no K-weighting, no gating, spectra not loudness-normalised, a constant
 * envelope correlation, a blind click counter, a ranking that ignores clicks or counts a negative delta,
 * a batch that does not level-match or takes several phrases of a preset) and must turn at least one row
 * red; a control that turns nothing red is itself a failure. No clock, no Math.random: mulberry32 only.
 *
 * Deterministic. No dependency but Node's own and tools/click_detector.mjs.
 */
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { tmpdir } from 'node:os';
import { dirname, join, relative, resolve, sep } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { detectClicks, readWav } from './click_detector.mjs';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const LOG10 = Math.log10;

/* ---------------------------------------------------------------- the seeded stream */
// the same stream as tools/click_detector_check.mjs's mulberry32 (not imported: that is a check script)
export function mulberry32(a) {
  return () => {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/* ---------------------------------------------------------------- 1. loudness (BS.1770-4) */
export const ABS_GATE = -70, REL_GATE = -10, OFFSET = -0.691;

/* The two pre-filter stages, from the analogue prototypes the standard's 48 kHz table was derived from
   (so the coefficients are right at 44.1 kHz and any other rate, where the table is not). */
export function kWeightStages(sr) {
  const shelf = (() => {
    const f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
    const K = Math.tan(Math.PI * f0 / sr), Vh = 10 ** (G / 20), Vb = Vh ** 0.4996667741545416, a0 = 1 + K / Q + K * K;
    return { b: [(Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0], a: [2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0] };
  })();
  const rlb = (() => {
    const f0 = 38.13547087602444, Q = 0.5003270373238773;
    const K = Math.tan(Math.PI * f0 / sr), a0 = 1 + K / Q + K * K;
    return { b: [1, -2, 1], a: [2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0] };
  })();
  return [shelf, rlb];
}

function biquad(x, { b, a }) {   // transposed direct form II, float64
  const y = new Float64Array(x.length);
  let z1 = 0, z2 = 0;
  for (let i = 0; i < x.length; i++) {
    const v = x[i], o = b[0] * v + z1;
    z1 = b[1] * v - a[0] * o + z2; z2 = b[2] * v - a[1] * o;
    y[i] = o;
  }
  return y;
}

/* |H| of the two stages at f, evaluated on the unit circle: the independent path the self-test checks the
   time-domain filtering against. */
export function kWeightGain(f, sr) {
  const w = 2 * Math.PI * f / sr, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
  let g = 1;
  for (const { b, a } of kWeightStages(sr)) {
    const nr = b[0] + b[1] * c1 + b[2] * c2, ni = -(b[1] * s1 + b[2] * s2);
    const dr = 1 + a[0] * c1 + a[1] * c2, di = -(a[0] * s1 + a[1] * s2);
    g *= Math.hypot(nr, ni) / Math.hypot(dr, di);
  }
  return g;
}

/* -> { lufs } or { lufs: null, reason }. Mono counts as one channel at weight 1, stereo as two (the weights
   of L and R); more channels are refused rather than guessed at (the surround weights are not needed here). */
export function loudness(chans, sr, { kWeight = true, gate = true } = {}) {
  if (chans.length < 1 || chans.length > 2) throw new Error(`loudness: ${chans.length} channels (this tool takes mono or stereo)`);
  const n = chans[0].length, blockLen = Math.round(0.4 * sr), step = Math.round(0.1 * sr);
  if (n < blockLen) return { lufs: null, reason: 'shorter than one 400 ms block' };
  const stages = kWeight ? kWeightStages(sr) : [];
  const cum = chans.map(x => {   // running sum of squares of the filtered channel: a block's energy is two lookups
    let y = x;
    for (const s of stages) y = biquad(y, s);
    const c = new Float64Array(n + 1);
    for (let i = 0; i < n; i++) c[i + 1] = c[i] + y[i] * y[i];
    return c;
  });
  const z = [];
  for (let s = 0; s + blockLen <= n; s += step) {
    let e = 0;
    for (const c of cum) e += (c[s + blockLen] - c[s]) / blockLen;
    z.push(e);
  }
  const lk = e => OFFSET + 10 * LOG10(e);
  if (!gate) {
    const m = z.reduce((a, b) => a + b, 0) / z.length;
    return m > 0 ? { lufs: lk(m) } : { lufs: null, reason: 'silent' };
  }
  const loud = z.filter(e => e > 0 && lk(e) > ABS_GATE);
  if (!loud.length) return { lufs: null, reason: `too quiet: no 400 ms block above the ${ABS_GATE} LUFS absolute gate` };
  const rel = lk(loud.reduce((a, b) => a + b, 0) / loud.length) + REL_GATE;
  const kept = loud.filter(e => lk(e) > rel);   // never empty: the block that sets the mean's maximum is over a level 10 LU below it
  return { lufs: lk(kept.reduce((a, b) => a + b, 0) / kept.length) };
}

/* ---------------------------------------------------------------- 2. third-octave spectrum */
export const BAND_N = Array.from({ length: 29 }, (_, i) => i - 16);                // 10^(n/10) x 1 kHz, n = -16 .. 12
export const BAND_FC = BAND_N.map(n => 1000 * 10 ** (n / 10));
export const BAND_NOMINAL = [25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000];
export const SPECTRAL_FLOOR_DB = -90;
const WELCH_N = 16384;   // 2.7 Hz bins at 44.1 kHz: the 25 Hz band (22 to 28 Hz) still holds two bins

const twiddles = new Map();
function fft(re, im) {   // in-place radix-2, forward
  const n = re.length;
  let t = twiddles.get(n);
  if (!t) {
    t = { c: new Float64Array(n / 2), s: new Float64Array(n / 2) };
    for (let k = 0; k < n / 2; k++) { t.c[k] = Math.cos(2 * Math.PI * k / n); t.s[k] = -Math.sin(2 * Math.PI * k / n); }
    twiddles.set(n, t);
  }
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { [re[i], re[j]] = [re[j], re[i]]; [im[i], im[j]] = [im[j], im[i]]; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const half = len >> 1, stride = n / len;
    for (let i = 0; i < n; i += len) {
      for (let k = 0, w = 0; k < half; k++, w += stride) {
        const a = i + k, b = a + half, xr = re[b] * t.c[w] - im[b] * t.s[w], xi = re[b] * t.s[w] + im[b] * t.c[w];
        re[b] = re[a] - xr; im[b] = im[a] - xi; re[a] += xr; im[a] += xi;
      }
    }
  }
}

/* The raw dB (10 log10 of mean-square power, channels summed like the loudness does) of each of the 29
   bands, from a Welch average. Two channels in ONE complex FFT (L real, R imaginary): |XL|^2 + |XR|^2 at bin
   k is (|Z[k]|^2 + |Z[N-k]|^2) / 2. */
export function bandLevels(chans, sr) {
  const N = WELCH_N, hop = N / 2, n = chans[0].length;
  const w = new Float64Array(N);
  let w2 = 0;
  for (let i = 0; i < N; i++) { w[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / N); w2 += w[i] * w[i]; }
  const starts = [];
  for (let s = 0; s + N <= n; s += hop) starts.push(s);
  if (!starts.length) starts.push(0);                  // shorter than one frame: one zero-padded frame
  const P = new Float64Array(N / 2 + 1), re = new Float64Array(N), im = new Float64Array(N);
  for (const s of starts) {
    for (let i = 0; i < N; i++) {
      const q = s + i;
      re[i] = q < n ? chans[0][q] * w[i] : 0;
      im[i] = chans.length > 1 && q < n ? chans[1][q] * w[i] : 0;
    }
    fft(re, im);
    for (let k = 0; k <= N / 2; k++) {
      const m = (N - k) % N;
      const p = chans.length > 1 ? (re[k] * re[k] + im[k] * im[k] + re[m] * re[m] + im[m] * im[m]) / 2 : re[k] * re[k] + im[k] * im[k];
      P[k] += (k === 0 || k === N / 2 ? 1 : 2) * p / (N * w2);   // one-sided: mean-square per bin (Parseval, window-normalised)
    }
  }
  const binHz = sr / N;
  return BAND_FC.map(fc => {
    const lo = fc * 10 ** -0.05, hi = fc * 10 ** 0.05;
    let sum = 0;
    for (let k = Math.ceil(lo / binHz); k * binHz < hi && k <= N / 2; k++) sum += P[k];
    return 10 * LOG10(Math.max(sum / starts.length, 1e-30));
  });
}

/* Each band relative to its render's K-weighted mean square (the loudness without its -0.691 offset), floored. */
const relativeBands = (bands, lufs) => bands.map(b => Math.max(b - (lufs - OFFSET), SPECTRAL_FLOOR_DB));

export function spectralDistance(bandsA, lufsA, bandsB, lufsB) {
  const a = relativeBands(bandsA, lufsA), b = relativeBands(bandsB, lufsB);
  let sum = 0, worst = -1, at = 0;
  for (let i = 0; i < a.length; i++) {
    const d = Math.abs(b[i] - a[i]);
    sum += d;
    if (d > worst) { worst = d; at = i; }
  }
  return { mean_abs_db: sum / a.length, worst_db: worst, worst_band_hz: BAND_NOMINAL[at] };
}

/* ---------------------------------------------------------------- 3. envelope */
export const ENV_FRAME_S = 0.01, ENV_FLOOR_DB = -90;

/* RMS (channels pooled) per 10 ms frame, in dB, floored; `gain` scales the render first. */
export function envelopeDb(chans, sr, gain = 1) {
  const f = Math.round(ENV_FRAME_S * sr), frames = Math.floor(chans[0].length / f), out = new Float64Array(frames);
  for (let j = 0; j < frames; j++) {
    let e = 0;
    for (const x of chans) for (let i = j * f; i < (j + 1) * f; i++) e += x[i] * x[i];
    out[j] = Math.max(10 * LOG10(Math.max(e / (f * chans.length), 1e-30) * gain * gain), ENV_FLOOR_DB);
  }
  return out;
}

export function envelopeCompare(a, b, sr) {   // a, b: dB envelopes; common length
  const n = Math.min(a.length, b.length);
  let ma = 0, mb = 0, same = true, maxd = -1, at = 0;
  for (let i = 0; i < n; i++) {
    ma += a[i]; mb += b[i];
    if (a[i] !== b[i]) same = false;
    const d = Math.abs(b[i] - a[i]);
    if (d > maxd) { maxd = d; at = i; }
  }
  ma /= n; mb /= n;
  let sab = 0, saa = 0, sbb = 0;
  for (let i = 0; i < n; i++) { sab += (a[i] - ma) * (b[i] - mb); saa += (a[i] - ma) ** 2; sbb += (b[i] - mb) ** 2; }
  const flatA = saa < 1e-12, flatB = sbb < 1e-12;
  const r = same ? 1 : flatA && flatB ? 1 : flatA || flatB ? 0 : Math.min(1, sab / Math.sqrt(saa * sbb));
  return { correlation: r, max_diff_db: maxd, max_diff_s: at * ENV_FRAME_S, frames: n };
}

/* ---------------------------------------------------------------- 4. clicks */
const clickEvents = (x, sr) => detectClicks(x, sr).length;

/* ---------------------------------------------------------------- the measures as one swappable set */
// The self-test swaps one entry for a broken one (its must-fail controls); nothing else reads these by name.
export const IMPL = {
  loudness: (chans, sr) => loudness(chans, sr),
  bandLevels,
  spectralDistance,
  envelopeCompare,
  clickEvents,
  matchGain: (legacyLufs, newLufs) => 10 ** ((legacyLufs - newLufs) / 20),
  terms: rankTerms,
  pickBatch,
};

/* ---------------------------------------------------------------- one pair */
export function analysePair(leg, neu, impl = IMPL) {
  if (leg.sr !== neu.sr) throw new Error(`rate mismatch: legacy ${leg.sr} Hz, new ${neu.sr} Hz`);
  if (leg.chans.length !== neu.chans.length) throw new Error(`channel mismatch: legacy ${leg.chans.length}, new ${neu.chans.length}`);
  for (const w of [leg, neu]) for (const x of w.chans) for (let i = 0; i < x.length; i++) if (!Number.isFinite(x[i])) throw new Error('non-finite sample in a render');
  const sr = leg.sr, rec = { rate: sr, frames_legacy: leg.chans[0].length, frames_new: neu.chans[0].length };
  const ll = impl.loudness(leg.chans, sr), ln = impl.loudness(neu.chans, sr);
  rec.loudness = { legacy_lufs: ll.lufs, new_lufs: ln.lufs, delta_lu: null };
  if (ll.lufs === null || ln.lufs === null) {
    rec.status = 'unmeasured';
    rec.reason = [ll.lufs === null ? `legacy ${ll.reason}` : null, ln.lufs === null ? `new ${ln.reason}` : null].filter(Boolean).join('; ');
    return rec;
  }
  rec.status = 'ok';
  rec.loudness.delta_lu = ln.lufs - ll.lufs;
  rec.spectral = impl.spectralDistance(impl.bandLevels(leg.chans, sr), ll.lufs, impl.bandLevels(neu.chans, sr), ln.lufs);
  const gain = impl.matchGain(ll.lufs, ln.lufs);
  rec.envelope = impl.envelopeCompare(envelopeDb(leg.chans, sr), envelopeDb(neu.chans, sr, gain), sr);
  const cl = leg.chans.map(x => impl.clickEvents(x, sr)), cn = neu.chans.map(x => impl.clickEvents(x, sr));
  rec.clicks = { legacy: cl.reduce((a, b) => a + b, 0), new: cn.reduce((a, b) => a + b, 0), per_channel_delta: cn.map((v, c) => v - cl[c]) };
  rec.clicks.delta = rec.clicks.new - rec.clicks.legacy;
  return rec;
}

/* ---------------------------------------------------------------- ranking */
const rankNorm = (v, all) => all.length < 2 ? 0 : all.filter(u => u < v).length / (all.length - 1);   // strictly smaller: ties are not ranks

/* The four terms of every measurable record (the rule in the header). Kept apart from the sum so a control
   can break one term. */
export function rankTerms(recs) {
  const loud = recs.map(r => Math.abs(r.loudness.delta_lu)), spec = recs.map(r => r.spectral.mean_abs_db);
  const env = recs.map(r => 1 - r.envelope.correlation), clk = recs.map(r => Math.max(0, r.clicks.delta));
  return recs.map((r, i) => ({ loudness: rankNorm(loud[i], loud), spectral: rankNorm(spec[i], spec), envelope: rankNorm(env[i], env), clicks: rankNorm(clk[i], clk) }));
}

/* Scores the 'ok' records in place and returns them ranked, most different first. */
export function rankRecords(records, impl = IMPL) {
  const ok = records.filter(r => r.status === 'ok'), terms = impl.terms(ok);
  ok.forEach((r, i) => { r.terms = terms[i]; r.score = terms[i].loudness + terms[i].spectral + terms[i].envelope + terms[i].clicks; });
  ok.sort((a, b) => b.score - a.score || (a.preset_id < b.preset_id ? -1 : a.preset_id > b.preset_id ? 1 : a.phrase < b.phrase ? -1 : a.phrase > b.phrase ? 1 : 0));
  ok.forEach((r, i) => { r.rank = i + 1; });
  return ok;
}

/* The top n of a ranked list, one pair per preset (each preset's best phrase is its first in rank order). */
export function pickBatch(ranked, n) {
  const seen = new Set(), out = [];
  for (const r of ranked) {
    if (out.length >= n) break;
    if (seen.has(r.preset_id)) continue;
    seen.add(r.preset_id); out.push(r);
  }
  return out;
}

/* ---------------------------------------------------------------- WAV out (24-bit PCM, any channel count) */
const FULL = 8388608, LIMIT = (FULL - 1) / FULL;   // 2^23; the largest positive 24-bit sample
export function encodeWav24(chans, sr, gain = 1) {
  const ch = chans.length, n = chans[0].length, data = n * ch * 3, buf = Buffer.alloc(44 + data);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + data, 4); buf.write('WAVEfmt ', 8); buf.writeUInt32LE(16, 16);
  buf.writeUInt16LE(1, 20); buf.writeUInt16LE(ch, 22); buf.writeUInt32LE(sr, 24); buf.writeUInt32LE(sr * ch * 3, 28);
  buf.writeUInt16LE(ch * 3, 32); buf.writeUInt16LE(24, 34); buf.write('data', 36); buf.writeUInt32LE(data, 40);
  for (let i = 0, p = 44; i < n; i++) for (let c = 0; c < ch; c++, p += 3) {
    const q = Math.round(chans[c][i] * gain * FULL);
    buf.writeIntLE(q > FULL - 1 ? FULL - 1 : q < -FULL ? -FULL : q, p, 3);
  }
  return buf;
}
const peakOf = chans => { let m = 0; for (const x of chans) for (let i = 0; i < x.length; i++) { const v = Math.abs(x[i]); if (v > m) m = v; } return m; };

/* ---------------------------------------------------------------- the run */
const STEM = /^(.*)__([^_].*)\.wav$/;   // <preset id>__<phrase>.wav (engine2_reference_renders.mjs)

function listWavs(dir) { return readdirSync(dir).filter(f => f.toLowerCase().endsWith('.wav')).sort(); }

function readManifest(dir) {
  for (const name of ['manifest.json', 'manifest-48k.json']) {
    const p = join(dir, name);
    if (existsSync(p)) return JSON.parse(readFileSync(p, 'utf8')).renders ?? [];
  }
  return null;
}

/* Which side's files are paired: the legacy manifest says bank or envelope; without one (or for a name it
   does not list) the id prefix does (bank-NN-..., env-NN-...). */
function classify(files, manifest) {
  const info = new Map((manifest ?? []).map(e => [e.file, e]));
  return files.map(file => {
    const e = info.get(file), m = STEM.exec(file);
    const preset_id = e?.preset_id ?? (m ? m[1] : file), phrase = e?.phrase ?? (m ? m[2] : '?');
    return { file, preset_id, preset: e?.preset ?? preset_id, phrase, bank: e ? e.source === 'bank' : preset_id.startsWith('bank-') };
  });
}

export function run({ legacyDir, newDir, outDir, batch = 20, seed = 1, impl = IMPL }) {
  const out = resolve(outDir), rel = relative(ROOT, out);
  if (!rel.startsWith('..') && !(rel === 'local' || rel.startsWith(`local${sep}`))) throw new Error(`--out ${outDir} is inside the repository and not under local/ (gitignored): the report is never written to a tracked path`);
  const man = readManifest(legacyDir), legFiles = listWavs(legacyDir), newFiles = listWavs(newDir);
  const haveLeg = new Set(legFiles), haveNew = new Set(newFiles);
  // The universe of bank pairs is every bank file named by the legacy manifest, the legacy directory or the new
  // directory, so a legacy set that is itself incomplete is reported, not silently shortened.
  const names = [...new Set([...legFiles, ...newFiles, ...(man ?? []).map(e => e.file)])].sort(), all = classify(names, man);
  const skipped = all.filter(e => !e.bank && haveLeg.has(e.file)).map(e => e.file);   // envelope presets present in the legacy set
  const pairs = all.filter(e => e.bank);
  const missing = { from_new: pairs.filter(e => !haveNew.has(e.file)).map(e => e.file), from_legacy: pairs.filter(e => !haveLeg.has(e.file)).map(e => e.file) };
  const records = [], errors = [];
  for (const e of pairs) {
    if (!haveNew.has(e.file) || !haveLeg.has(e.file)) continue;
    const base = { file: e.file, preset_id: e.preset_id, preset: e.preset, phrase: e.phrase };
    try {
      records.push({ ...base, ...analysePair(readWav(readFileSync(join(legacyDir, e.file))), readWav(readFileSync(join(newDir, e.file))), impl) });
    } catch (err) { errors.push({ ...base, status: 'error', reason: String(err.message || err) }); }
  }
  const ranked = rankRecords(records, impl), unmeasured = records.filter(r => r.status === 'unmeasured');
  const picks = impl.pickBatch(ranked, batch);
  mkdirSync(join(out, 'batch'), { recursive: true });
  for (const f of readdirSync(join(out, 'batch'))) if (/^\d+-[ab]\.wav$|^key\.json$/.test(f)) rmSync(join(out, 'batch', f));   // a smaller batch must not sit among the last run's files
  const pad = Math.max(2, String(picks.length).length), draw = mulberry32(seed), key = [];
  picks.forEach((r, i) => {
    const nn = String(i + 1).padStart(pad, '0'), L = readWav(readFileSync(join(legacyDir, r.file))), N = readWav(readFileSync(join(newDir, r.file)));
    let g = impl.matchGain(r.loudness.legacy_lufs, r.loudness.new_lufs), s = 1;
    const peak = Math.max(peakOf(L.chans), peakOf(N.chans) * g);
    if (peak > LIMIT) s = LIMIT / peak;
    const aIsLegacy = draw() < 0.5, A = aIsLegacy ? [L, s] : [N, g * s], B = aIsLegacy ? [N, g * s] : [L, s];
    writeFileSync(join(out, 'batch', `${nn}-a.wav`), encodeWav24(A[0].chans, A[0].sr, A[1]));
    writeFileSync(join(out, 'batch', `${nn}-b.wav`), encodeWav24(B[0].chans, B[0].sr, B[1]));
    r.batch = { nn, scale_db: 20 * LOG10(s), new_gain_db: 20 * LOG10(g), clip_safe_scaled: s < 1 };
    key.push({ nn, preset_id: r.preset_id, preset: r.preset, phrase: r.phrase, a: aIsLegacy ? 'legacy' : 'new', b: aIsLegacy ? 'new' : 'legacy', new_scaled_to_legacy_by_db: 20 * LOG10(g), both_scaled_down_by_db: 20 * LOG10(s) });
  });
  writeFileSync(join(out, 'batch', 'key.json'), JSON.stringify({ note: 'THE ANSWER SHEET: do not open before listening. a/b says which file of each pair is the legacy render and which the new render (scaled to the legacy integrated loudness).', seed, pairs: key }, null, 1));
  const header = { schema: 'horde.engine2.differential/1', legacy_dir: legacyDir, new_dir: newDir, batch_cap: batch, seed, rule: RULE, pairs_measured: ranked.length, preset_count: new Set(ranked.map(r => r.preset_id)).size };
  const report = { ...header, ranked, unmeasured, errors, missing, skipped_envelope: skipped };
  writeFileSync(join(out, 'report.json'), JSON.stringify(report, null, 1));
  writeFileSync(join(out, 'report.md'), markdown(header, ranked, picks, unmeasured, errors, missing, skipped));
  return { report, picks, ranked, unmeasured, errors, missing, skipped, outDir: out };
}

/* ---------------------------------------------------------------- report.md */
export const RULE = 'For each of four measures (|loudness delta|, spectral mean absolute difference, 1 - envelope correlation, max(0, click delta)) a pair\'s term is its rank-normalised position over all measured pairs: (pairs with a strictly smaller value) / (pairs - 1), 0 to 1. Score = the sum of the four terms (0 to 4). Ranks need no tuned weights between four different units and one outlier cannot swamp the rest; equal weights are a choice, not a finding. Clicks count only when the new engine has more. Batch = the top pairs, at most one per preset (its highest-scoring phrase). A report, never a gate.';
const f = (v, d = 2) => v === null || v === undefined ? 'n/a' : v.toFixed(d);
const sgn = (v, d = 2) => v === null ? 'n/a' : (v > 0 ? '+' : '') + v.toFixed(d);

function row(r) {
  return `| ${r.rank} | ${r.preset} | ${r.phrase} | ${sgn(r.loudness.delta_lu)} | ${f(r.spectral.mean_abs_db)} (${r.spectral.worst_db.toFixed(1)} @ ${r.spectral.worst_band_hz} Hz) | ${r.envelope.correlation.toFixed(3)} (${r.envelope.max_diff_db.toFixed(1)} dB @ ${r.envelope.max_diff_s.toFixed(2)} s) | ${sgn(r.clicks.delta, 0)} (${r.clicks.legacy} -> ${r.clicks.new}) | ${r.score.toFixed(3)} |`;
}
const HEAD = '| rank | preset | phrase | loudness delta (LU) | spectral mean abs (worst band) dB | envelope r (largest frame diff) | clicks delta (legacy -> new) | score |\n|---:|---|---|---:|---|---|---|---:|';

function markdown(h, ranked, picks, unmeasured, errors, missing, skipped) {
  const L = [];
  L.push('# Differential report: new engine against legacy', '',
    `${h.pairs_measured} pairs measured over ${h.preset_count} bank presets. Reported, never a gate (B523, ADR-247 audit P8, spec 10.6 item 3).`, '',
    `**The difference score.** ${h.rule}`, '',
    '**Measures.** Loudness: BS.1770-4 integrated, new minus legacy. Spectral: 1/3-octave bands 25 Hz to 16 kHz, each render relative to its own loudness (timbre, not level). Envelope: 10 ms RMS in dB, new scaled to the legacy loudness first; r = 1 is the same shape. Clicks: tools/click_detector.mjs events summed over channels (it does not judge the first 0.12 s, so a click at the onset of either engine is not counted).', '',
    `## The listening batch (${picks.length} of at most ${h.batch_cap}; seed ${h.seed})`, '',
    'Files are in `batch/` as `NN-a.wav` and `NN-b.wav`, the new render scaled to the legacy loudness. Which of a and b is which is in `batch/key.json` only; open it after listening.', '', HEAD);
  for (const r of picks) L.push(row(r) + (r.batch.clip_safe_scaled ? ` (pair ${r.batch.nn}: both files scaled down ${r.batch.scale_db.toFixed(2)} dB so the louder stays under full scale)` : ''));
  if (!picks.length) L.push('| (no measured pairs) | | | | | | | |');
  L.push('', 'Batch numbering: ' + picks.map(r => `${r.batch.nn} = rank ${r.rank}`).join(', '), '', `## All ${ranked.length} measured pairs, most different first`, '', HEAD);
  for (const r of ranked) L.push(row(r));
  L.push('', `## Missing (${missing.from_new.length + missing.from_legacy.length})`, '');
  L.push(`Missing from the new renders (${missing.from_new.length}): ${missing.from_new.length ? missing.from_new.join(', ') : 'none'}`, '');
  L.push(`Missing from the legacy set (${missing.from_legacy.length}): ${missing.from_legacy.length ? missing.from_legacy.join(', ') : 'none'}`, '');
  L.push(`## Unmeasured (${unmeasured.length}): paired, but not rankable or level-matchable; look at these by hand`, '', ...(unmeasured.length ? unmeasured.map(r => `- ${r.file}: ${r.reason}`) : ['none']), '');
  L.push(`## Errors (${errors.length})`, '', ...(errors.length ? errors.map(r => `- ${r.file}: ${r.reason}`) : ['none']), '');
  L.push(`## Skipped: envelope presets (${skipped.length} files)`, '', skipped.length ? 'The differential judges the bank presets only. Not compared: ' + skipped.join(', ') : 'none', '');
  return L.join('\n');
}

/* ---------------------------------------------------------------- CLI */
function cli(argv) {
  const opt = n => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : undefined; };
  if (argv.includes('--selftest')) return selfTest();
  const legacyDir = opt('--legacy'), newDir = opt('--new'), outDir = opt('--out');
  if (!legacyDir || !newDir || !outDir) { console.error('usage: node tools/engine2_differential.mjs --legacy <dir> --new <dir> --out <dir> [--batch 20] [--seed 1] | --selftest'); return 64; }
  for (const d of [legacyDir, newDir]) if (!existsSync(d)) { console.error(`engine2_differential: no such directory ${d}`); return 64; }
  const batch = Number(opt('--batch') ?? 20), seed = Number(opt('--seed') ?? 1);
  if (!Number.isInteger(batch) || batch < 1 || !Number.isInteger(seed)) { console.error('engine2_differential: --batch and --seed take integers (batch >= 1)'); return 64; }
  const r = run({ legacyDir, newDir, outDir, batch, seed });
  console.log(`engine2_differential: ${r.ranked.length} pairs measured, ${r.picks.length} in the batch, ${r.unmeasured.length} unmeasured, ${r.errors.length} errors, ${r.missing.from_new.length} missing from new, ${r.missing.from_legacy.length} missing from legacy, ${r.skipped.length} envelope files skipped`);
  console.log(`engine2_differential: wrote report.md, report.json and batch/ (${r.picks.length} pairs + key.json) under ${r.outDir}`);
  return 0;
}

/* ================================================================ the self-test */
const SR = 48000;
const wavHeader = (ch, bits, tag, n) => {
  const B = bits / 8, data = n * ch * B, h = Buffer.alloc(44);
  h.write('RIFF', 0); h.writeUInt32LE(36 + data, 4); h.write('WAVEfmt ', 8); h.writeUInt32LE(16, 16);
  h.writeUInt16LE(tag, 20); h.writeUInt16LE(ch, 22); h.writeUInt32LE(SR, 24); h.writeUInt32LE(SR * ch * B, 28);
  h.writeUInt16LE(ch * B, 32); h.writeUInt16LE(bits, 34); h.write('data', 36); h.writeUInt32LE(data, 40);
  return h;
};
/* the writers the fixtures need beyond encodeWav24: 16-bit PCM and 32-bit float, to prove the reader's variants */
function encodeWav16(chans) {
  const n = chans[0].length, ch = chans.length, d = Buffer.alloc(n * ch * 2);
  for (let i = 0, p = 0; i < n; i++) for (let c = 0; c < ch; c++, p += 2) d.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(chans[c][i] * 32768))), p);
  return Buffer.concat([wavHeader(ch, 16, 1, n), d]);
}
function encodeWavF32(chans) {
  const n = chans[0].length, ch = chans.length, d = Buffer.alloc(n * ch * 4);
  for (let i = 0, p = 0; i < n; i++) for (let c = 0; c < ch; c++, p += 4) d.writeFloatLE(chans[c][i], p);
  return Buffer.concat([wavHeader(ch, 32, 3, n), d]);
}

/* Tones at multiples of 187.5 Hz, which is both a multiple of 2.5 Hz (every difference frequency then has a
   whole number of cycles in a 400 ms block, so a block's mean square is exactly the sum of the tones' own and
   the analytic loudness below is exact) and a whole number of the Welch bins (sr / 16384 = 2.93 Hz; a tone on a
   bin leaks into its two neighbours only, under the periodic Hann window, so no other band holds a trace of
   it). One tone per band: the 200, 400, 1600 and 6300 Hz bands. */
const TONES = [187.5, 375, 1500, 6000], TONE_BANDS = [200, 400, 1600, 6300];
const tones = (amps, secs = 1.0, f0 = TONES) => {
  const n = Math.round(secs * SR), x = new Float64Array(n);
  f0.forEach((fr, k) => { for (let i = 0; i < n; i++) x[i] += amps[k] * Math.sin(2 * Math.PI * fr * i / SR); });
  return x;
};
const CLICK_AT = Math.round(0.6 * SR);   // well past the click detector's 0.12 s of warm-up
const stereo = x => [x, Float64Array.from(x)];
const scaleBy = (chans, g) => chans.map(x => x.map(v => v * g));
const withEnvelope = (chans, env) => chans.map(x => x.map((v, i) => v * env(i / SR)));
const withBlip = (chans, at, h, len = 8) => chans.map((x, c) => { const y = Float64Array.from(x); if (c === 0) for (let i = 0; i < len; i++) y[at + i] += h; return y; });
const db = d => 10 ** (d / 20);
const analyticLufs = (amps, ch = 2) => OFFSET + 10 * LOG10(ch * TONES.reduce((s, fr, k) => s + (kWeightGain(fr, SR) * amps[k]) ** 2 / 2, 0));

/* The fixtures, built once and shared by the battery and every control. */
function fixtures() {
  const flat = [0.05, 0.05, 0.05, 0.05], base = stereo(tones(flat));
  const tiltAmps = flat.map((a, k) => a * db(-6 * k));                      // 0, -6, -12, -18 dB across four bands
  const tilt = stereo(tones(tiltAmps));
  const slow = withEnvelope(base, t => Math.min(1, t / 0.5)), fast = withEnvelope(base, t => Math.exp(-t * 3));
  return { flat, base, tiltAmps, tilt, slow, fast, loud: scaleBy(base, 2), clicked: withBlip(base, CLICK_AT, 0.15) };
}

/* the rows: [ok, tag, text]. `impl` is the measure set under test. */
function battery(impl, fx, work, { e2e = true, seed = true } = {}) {
  const rows = [], row = (ok, tag, text) => rows.push([!!ok, tag, text]);
  const near = (a, b, tol) => a !== null && Math.abs(a - b) <= tol;
  const A = (leg, neu) => analysePair({ sr: SR, chans: leg }, { sr: SR, chans: neu }, impl);

  // ---- loudness: the standard's own calibration, then the filter against its analytic response
  const sine997 = [tones([1], 1.0, [997]), new Float64Array(SR)];
  const lSine = impl.loudness(sine997, SR).lufs;
  row(near(lSine, -3.01, 0.05), 'LOUD-CAL', `a full-scale 997 Hz sine in one channel reads ${f(lSine, 3)} LUFS (BS.1770 / EBU Tech 3341 calibration: -3.01)`);
  const lBase = impl.loudness(fx.base, SR).lufs, aBase = analyticLufs(fx.flat);
  row(near(lBase, aBase, 0.03), 'LOUD-K', `the four-tone mix reads ${f(lBase, 3)} LUFS against ${f(aBase, 3)} from the K-filters' analytic response`);
  // 1.5 s of the same mix 30 dB down follows: under the relative gate (-10 LU), so it must not pull the reading
  // down. Not exact: the 400 ms blocks that straddle the join are over the gate (partly loud, and they count),
  // which is why the tolerance is 1 LU against the ungated reading's 4.
  const tail = scaleBy(fx.base, db(-30)), gated = fx.base.map((x, c) => { const y = new Float64Array(x.length + 3 * SR / 2); y.set(x); y.set(tail[c].subarray(0, 3 * SR / 2), x.length); return y; });
  const lGated = impl.loudness(gated, SR).lufs;
  row(near(lGated, lBase, 1.0), 'LOUD-GATE', `the mix followed by 1.5 s of itself 30 dB down reads ${f(lGated, 3)} LUFS, the mix alone ${f(lBase, 3)} (within 1.0: the relative gate drops the tail; without it the reading is about 4 dB lower)`);
  const quiet = impl.loudness(scaleBy(fx.base, db(-90)), SR);
  row(quiet.lufs === null && /quiet/.test(quiet.reason ?? ''), 'LOUD-QUIET', `a render 90 dB down reads "${quiet.reason}", not a number`);

  // ---- identical, and 6 dB apart
  const same = A(fx.base, fx.base);
  row(near(same.loudness.delta_lu, 0, 1e-9) && near(same.spectral.mean_abs_db, 0, 1e-9) && near(same.envelope.correlation, 1, 1e-9) && same.clicks.delta === 0,
    'IDENTICAL', `an identical pair reads ${f(same.loudness.delta_lu, 4)} LU, ${f(same.spectral.mean_abs_db, 4)} dB, correlation ${f(same.envelope.correlation, 6)}, click delta ${same.clicks.delta}`);
  const six = A(fx.base, fx.loud);
  row(near(six.loudness.delta_lu, 6.0, 0.1), 'SIX-DB', `a pair 6 dB apart reads ${sgn(six.loudness.delta_lu, 3)} LU (within 0.1 of +6.0)`);
  row(near(six.spectral.mean_abs_db, 0, 0.01) && near(six.envelope.correlation, 1, 1e-6), 'LEVEL-FREE', `the same pair reads ${f(six.spectral.mean_abs_db, 4)} dB spectral distance and correlation ${f(six.envelope.correlation, 6)}: a level offset is not a timbre or a shape difference`);

  // ---- a known tilt: the expected distance from the analytic levels (band power A^2 per stereo pair, loudness as above)
  const rel = (amps, k) => 10 * LOG10(amps[k] ** 2) - (analyticLufs(amps) - OFFSET);   // a tone's band: mean square a^2/2 per channel, two channels, re the K-weighted mean square
  const diffs = TONES.map((_, k) => Math.abs(rel(fx.tiltAmps, k) - rel(fx.flat, k))), expected = diffs.reduce((a, b) => a + b, 0) / BAND_FC.length;   // the 25 empty bands sit on the floor in both
  const tilt = A(fx.base, fx.tilt);
  row(near(tilt.spectral.mean_abs_db, expected, 0.1), 'TILT', `a 0 / -6 / -12 / -18 dB tilt across four bands reads ${f(tilt.spectral.mean_abs_db, 3)} dB mean (expected ${f(expected, 3)}), worst band ${tilt.spectral.worst_band_hz} Hz by ${f(tilt.spectral.worst_db, 2)} dB`);
  const wantBand = TONE_BANDS[diffs.indexOf(Math.max(...diffs))];
  row(tilt.spectral.worst_band_hz === wantBand && near(tilt.spectral.worst_db, Math.max(...diffs), 0.1), 'TILT-WORST', `the worst band of that tilt is ${wantBand} Hz by ${f(Math.max(...diffs), 2)} dB (analytic): read ${tilt.spectral.worst_band_hz} Hz by ${f(tilt.spectral.worst_db, 2)} dB`);

  // ---- envelope
  const attack = A(fx.slow, fx.fast);
  row(attack.envelope.correlation < 0.9 && attack.envelope.max_diff_db > 6, 'ENVELOPE', `a slow attack against an instant one with a decay reads r ${f(attack.envelope.correlation, 3)} (< 0.9), largest frame difference ${f(attack.envelope.max_diff_db, 1)} dB at ${f(attack.envelope.max_diff_s, 2)} s`);

  // ---- clicks
  const clk = A(fx.base, fx.clicked), clkRev = A(fx.clicked, fx.base);
  row(clk.clicks.delta > 0 && clk.clicks.legacy === 0, 'CLICK', `a planted click in the new render reads click delta ${sgn(clk.clicks.delta, 0)} (legacy ${clk.clicks.legacy}, new ${clk.clicks.new})`);
  row(clkRev.clicks.delta < 0, 'CLICK-SIGN', `the same click in the LEGACY render reads ${sgn(clkRev.clicks.delta, 0)} (new minus legacy)`);

  // ---- the ranking rule on bare numbers: only the click term differs, and a negative delta is no difference
  const mk = (id, d) => ({ preset_id: id, phrase: 'held', status: 'ok', loudness: { delta_lu: 0 }, spectral: { mean_abs_db: 1 }, envelope: { correlation: 1 }, clicks: { delta: d } });
  const rk = rankRecords([mk('bank-a', 0), mk('bank-b', 5), mk('bank-c', -3)], impl);
  row(rk[0].preset_id === 'bank-b' && rk[0].score > 0 && rk[1].score === 0 && rk[2].score === 0, 'RANK-CLICK', `with only the click delta differing (0, +5, -3) the +5 pair ranks first (${f(rk[0].score, 3)}) and the 0 and -3 pairs score ${f(rk[1].score, 3)} and ${f(rk[2].score, 3)}`);

  if (!e2e) return rows;

  // ---- end to end on disk: ranking, missing pairs, the formats, the batch
  const e = work;
  const legDir = join(e, 'legacy'), newDir = join(e, 'new'), outDir = join(e, 'out');
  for (const d of [legDir, newDir]) mkdirSync(d, { recursive: true });
  const man = [], put = (dir, name, buf) => writeFileSync(join(dir, name), buf);
  const peaky = (() => { const c = scaleBy(fx.base, db(-25)); c[0][CLICK_AT] = 0.9; return c; })();
  const worst = withBlip(scaleBy(withEnvelope(fx.tilt, t => Math.exp(-t * 3)), 2), CLICK_AT, 0.15);
  const pairsSpec = [   // [preset id, phrase, legacy, new, new's writer]
    ['bank-01-same', 'held', fx.base, fx.base, encodeWav24],
    ['bank-02-gain', 'held', fx.base, fx.loud, encodeWavF32],
    ['bank-03-tilt', 'held', fx.base, fx.tilt, encodeWav16],
    ['bank-03-tilt', 'chord', fx.base, withBlip(fx.tilt, CLICK_AT, 0.15), encodeWav24],   // the same preset's more different phrase
    ['bank-04-attack', 'held', fx.slow, fx.fast, encodeWav24],
    ['bank-05-clicked', 'held', fx.base, fx.clicked, encodeWav24],
    ['bank-06-worst', 'held', fx.base, worst, encodeWav24],
    ['bank-07-peaky', 'held', scaleBy(fx.base, db(-20)), peaky, encodeWavF32],   // the new render must be scaled up past full scale to match: clip-safe
    ['bank-08-silent', 'held', fx.base, scaleBy(fx.base, db(-90)), encodeWav24],  // unmeasurable
  ];
  for (const [id, ph, lg, nw, enc] of pairsSpec) {
    const file = `${id}__${ph}.wav`;
    put(legDir, file, encodeWav24(lg, SR)); put(newDir, file, enc === encodeWav24 ? encodeWav24(nw, SR) : enc(nw));
    man.push({ file, preset_id: id, preset: id.replace(/^bank-\d+-/, ''), source: 'bank', phrase: ph });
  }
  // a rate mismatch, one pair only in legacy, one only in new, and an envelope preset (skipped, not missing)
  put(legDir, 'bank-09-rate__held.wav', encodeWav24(fx.base, SR)); put(newDir, 'bank-09-rate__held.wav', encodeWav24(fx.base, 44100));
  put(legDir, 'bank-10-gone__held.wav', encodeWav24(fx.base, SR));
  put(newDir, 'bank-11-extra__held.wav', encodeWav24(fx.base, SR));
  put(legDir, 'env-01-pluck__held.wav', encodeWav24(fx.base, SR));
  for (const [id, ph, src] of [['bank-09-rate', 'held', 'bank'], ['bank-10-gone', 'held', 'bank'], ['env-01-pluck', 'held', 'envelope']]) man.push({ file: `${id}__${ph}.wav`, preset_id: id, preset: id, source: src, phrase: ph });
  writeFileSync(join(legDir, 'manifest.json'), JSON.stringify({ renders: man }));

  let res = null, err = null;
  try { res = run({ legacyDir: legDir, newDir, outDir, batch: 20, seed: 7, impl }); } catch (x) { err = x; }
  if (!res) { row(false, 'RUN', `the run threw: ${err && err.message}`); return rows; }
  const order = res.ranked.map(r => `${r.preset_id}/${r.phrase}`);
  row(order[0] === 'bank-06-worst/held', 'RANK-FIRST', `the pair most different on all four measures ranks first: ${order[0]}`);
  row(res.ranked[res.ranked.length - 1].preset_id === 'bank-01-same' && res.ranked[res.ranked.length - 1].score === 0, 'RANK-LAST', `the identical pair ranks last with score ${f(res.ranked[res.ranked.length - 1].score, 3)}`);
  const clickRank = res.ranked.findIndex(r => r.preset_id === 'bank-05-clicked'), gainRank = res.ranked.findIndex(r => r.preset_id === 'bank-02-gain');
  row(clickRank >= 0 && gainRank >= 0 && clickRank < gainRank, 'RANK-CLICKS', `a click-only difference (rank ${clickRank + 1}) outranks a level-only one (rank ${gainRank + 1})`);
  row(res.ranked.length === 8 && res.unmeasured.length === 1 && res.unmeasured[0].preset_id === 'bank-08-silent' && res.errors.length === 1 && /rate mismatch/.test(res.errors[0].reason), 'PAIRS', `8 measured, the 90 dB-down new render unmeasured (${res.unmeasured.map(r => r.reason).join('|')}), the rate mismatch an error (${res.errors.map(r => r.reason).join('|')})`);
  row(res.missing.from_new.join() === 'bank-10-gone__held.wav' && res.missing.from_legacy.join() === 'bank-11-extra__held.wav' && res.skipped.join() === 'env-01-pluck__held.wav', 'MISSING', `missing named on both sides (${res.missing.from_new} / ${res.missing.from_legacy}), the envelope preset skipped (${res.skipped})`);
  const ids = res.picks.map(r => r.preset_id);
  row(new Set(ids).size === ids.length && res.picks.find(r => r.preset_id === 'bank-03-tilt')?.phrase === 'chord', 'BATCH-ONE', `one pair per preset (${ids.length} picks, ${new Set(ids).size} presets), the tilt preset's picked phrase is its more different one (${res.picks.find(r => r.preset_id === 'bank-03-tilt')?.phrase})`);
  // the blind files
  const bdir = join(outDir, 'batch'), keyFile = JSON.parse(readFileSync(join(bdir, 'key.json'), 'utf8'));
  let worstGap = 0, over = 0, nCheck = 0, scaledSeen = false;
  for (const k of keyFile.pairs) {
    const wa = readWav(readFileSync(join(bdir, `${k.nn}-a.wav`))), wb = readWav(readFileSync(join(bdir, `${k.nn}-b.wav`)));
    const la = impl.loudness(wa.chans, SR).lufs, lb = impl.loudness(wb.chans, SR).lufs;
    worstGap = Math.max(worstGap, Math.abs(la - lb)); over = Math.max(over, peakOf(wa.chans), peakOf(wb.chans)); nCheck++;
    if (k.both_scaled_down_by_db < -0.001) scaledSeen = true;
  }
  row(nCheck === res.picks.length && worstGap <= 0.1, 'BATCH-LEVEL', `${nCheck} batch pairs, the a and b files of each within ${f(worstGap, 4)} LU of each other (limit 0.1)`);
  row(over <= 1 && scaledSeen, 'BATCH-CLIP', `no batch sample passes full scale (peak ${f(over, 6)}), and the pair that would have (bank-07-peaky) was scaled down on both files`);
  const md = readFileSync(join(outDir, 'report.md'), 'utf8');
  row(/The difference score/.test(md) && /bank-06-worst|worst/.test(md) && !/"a":/.test(md) && /Skipped: envelope presets \(1 files\)/.test(md), 'REPORT', 'report.md states the rule, lists the batch and the skipped files, and does not carry the a/b key');
  if (!seed) return rows;
  const again = (() => { const o2 = join(e, 'out2'); run({ legacyDir: legDir, newDir, outDir: o2, batch: 20, seed: 7, impl }); return readFileSync(join(o2, 'batch', 'key.json'), 'utf8') === readFileSync(join(bdir, 'key.json'), 'utf8'); })();
  const other = (() => { const o3 = join(e, 'out3'); run({ legacyDir: legDir, newDir, outDir: o3, batch: 20, seed: 8, impl }); return readFileSync(join(o3, 'batch', 'key.json'), 'utf8') !== readFileSync(join(bdir, 'key.json'), 'utf8'); })();
  const mix = keyFile.pairs.some(k => k.a === 'legacy') && keyFile.pairs.some(k => k.a === 'new');
  row(again && other && mix, 'BATCH-SEED', `the same seed gives the same key (${again}), another seed another key (${other}), and a and b are both used for the legacy render (${mix})`);
  return rows;
}

/* A click detector call costs about a tenth of a second; the controls re-run the battery on the same
   signals, so the self-test memoises by content (only here: the run itself never caches). */
function memoClicks() {
  const cache = new Map();
  return (x, sr) => {
    const k = createHash('sha1').update(Buffer.from(x.buffer, x.byteOffset, x.byteLength)).update(String(sr)).digest('hex');
    if (!cache.has(k)) cache.set(k, clickEvents(x, sr));
    return cache.get(k);
  };
}

/* The deliberately broken measure sets. Each differs from IMPL in exactly one entry, names the rows that
   must see it (`expect`: it has to turn at least one of THOSE red, not just any), and says how much of the
   battery it needs: the end-to-end half (disk, ranking, batch) costs most of the time and only the batch
   controls reach it. */
function controls() {
  const units = { e2e: false, seed: false }, batch = { e2e: true, seed: false };
  return [
    ['loudness without K-weighting', { loudness: (c, sr) => loudness(c, sr, { kWeight: false }) }, ['LOUD-CAL', 'LOUD-K'], units],
    ['loudness without the gates', { loudness: (c, sr) => loudness(c, sr, { gate: false }) }, ['LOUD-GATE', 'LOUD-QUIET'], units],
    ['spectra not normalised to loudness', { spectralDistance: (a, la, b, lb) => spectralDistance(a, 0, b, 0) }, ['LEVEL-FREE', 'TILT'], units],
    ['an envelope correlation that is always 1', { envelopeCompare: (a, b, sr) => ({ ...envelopeCompare(a, b, sr), correlation: 1 }) }, ['ENVELOPE'], units],
    ['a click counter that sees nothing', { clickEvents: () => 0 }, ['CLICK'], units],
    ['a ranking that ignores clicks', { terms: recs => rankTerms(recs).map(t => ({ ...t, clicks: 0 })) }, ['RANK-CLICK'], units],
    ['a ranking that counts a negative click delta', { terms: recs => rankTerms(recs.map(r => ({ ...r, clicks: { ...r.clicks, delta: Math.abs(r.clicks.delta) } }))) }, ['RANK-CLICK'], units],
    ['a batch that does not level-match', { matchGain: () => 1 }, ['BATCH-LEVEL'], batch],
    ['a batch that takes several phrases of a preset', { pickBatch: (ranked, n) => ranked.slice(0, n) }, ['BATCH-ONE'], batch],
  ];
}

export function selfTest() {
  const fx = fixtures(), work = mkdtempSync(join(tmpdir(), 'e2diff-'));
  let red = 0;
  try {
    const clicks = memoClicks(), mkImpl = over => ({ ...IMPL, clickEvents: clicks, ...over });
    let n = 0;
    const sub = () => { const d = join(work, `b${n++}`); mkdirSync(d, { recursive: true }); return d; };
    const main = battery(mkImpl({}), fx, sub());
    for (const [ok, tag, text] of main) { console.log(`${ok ? 'PASS' : 'FAIL'}  ${tag.padEnd(11)} ${text}`); if (!ok) red++; }
    for (const [name, over, expect, opts] of controls()) {
      const rows = battery(mkImpl(over), fx, sub(), opts), failed = rows.filter(r => !r[0]).map(r => r[1]);
      const fired = expect.some(t => failed.includes(t));
      console.log(`${fired ? 'PASS' : 'FAIL'}  ${'MUSTFAIL'.padEnd(11)} ${name}: ${fired ? `turns ${failed.join(', ')} red` : `turned ${failed.length ? failed.join(', ') : 'NOTHING'} red, none of ${expect.join(', ')}: the battery cannot see this fault`}`);
      if (!fired) red++;
    }
    console.log(`engine2_differential --selftest: ${red ? 'RED' : 'GREEN'} (${red} row(s) failed; ${main.length} measure rows, ${controls().length} must-fail controls)`);
  } catch (e) {
    console.error(String(e && e.stack || e));
    console.log(`engine2_differential --selftest: RED (the run broke: ${String(e && e.message || e).split('\n')[0]})`);
    red = red || 1;
  } finally {
    rmSync(work, { recursive: true, force: true });
  }
  return red ? 1 : 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  try { process.exitCode = cli(process.argv.slice(2)); }
  catch (e) { console.error(String(e && e.message || e)); process.exitCode = 2; }
}
