/*
 * quadrant_cost_bench.cpp — what a "Quadrant" wavetable oscillator under the swarm would cost (B648).
 * HYPERSAW / horde, 2026-11-07. Origin: ROADMAP B648 (the human, by poll: "File it plus a cost bench"
 * for the Quadrant wavetable spec, ADR-384 CANDIDATE). The decision this feeds: should Quadrant
 * replace or supplement the blade (SCALPEL) oscillator, with a real CPU figure instead of the
 * spec's paper estimate ("~1-2% of one core, 8 voices, 48 kHz").
 *
 * NOT A GATE and NOT THE PORT. It implements the Quadrant core as the spec describes it, WITH the
 * lead's review amendments, so the number measured is for the design we would actually build:
 *   - 16-frame tables of complex harmonic coefficients, generated in-file (cost does not depend on
 *     the recipe; four rich-spectrum recipes stand in for the factory set).
 *   - H harmonics is a run-time option: 64 (the spec as written) or 1024 (the review's pick,
 *     2048-sample top cycle).
 *   - Blend laws Linear and Spectral log exactly as the spec's Blend math (bilinear weights, per-corner
 *     frame lerp first, eps 1e-4, log-magnitude CACHE, phase from the linear sum, heaviest-corner
 *     fallback below 1e-6).
 *   - Rebuild every 128 samples, only when an input changed; real IFFT into PER-OCTAVE levels (top
 *     harmonic halves per level), level size = 32 samples per period of its top harmonic, 64 floor,
 *     capped at --maxn (2048). Only the levels sounding voices use are built. Fixed 0.85 gain.
 *   - Crossfade between the old and new set lasts exactly the rebuild interval (128 samples).
 *   - Voice read: phase accumulator + linear interpolation; the mip pair is chosen by pitch and the
 *     two ADJACENT levels are crossfaded (continuous brightness on glides), so each read touches two
 *     levels (four while a set crossfade is running). The fade runs the safe way round: the pair is
 *     (kb, kb+1), kb = floor(pos)+1, pos = log2(H * f / min(20 kHz, 0.45 fs)), so no level above the
 *     limit is ever read (the cost is brightness: up to one octave of top harmonics fades early).
 *   - The swarm on top: M members per voice, each its own phase and slightly detuned frequency
 *     (+-0.6 %), all reading the same published set, with a minimal mean-field Kuramoto update once
 *     per 16-sample tick per voice (NOT horde's exact law; it exists so coupling's cost is in the
 *     number). Start phases from one seeded mulberry32 stream.
 * Harmonic h of a level is capped at N/2-1 (the Nyquist bin is dropped). At H=1024 with the 2048
 * cap the top levels carry 2-4 samples per period of their top harmonic, which linear interpolation
 * reads poorly: the spec's 32-per-period rule would want 32768 there (try --maxn 32768 for the
 * sensitivity; instructions grow with the IFFT sizes).
 *
 * FOLLOW-UP OPTIONS (B648 follow-up, 2026-11-07; the design specs/SPEC-WAVETABLE.md rows WT-5, 6, 6a,
 * 15, 16, 23, 25 specify, on the b649-wavetable-spec branch). Run-time options; the defaults are the
 * first run's design (the 2-point read compiles to the same expression, but the loop was restructured, so
 * the default cells now read ~2 % (moving) to ~15 % (static) above the first run's table):
 *   --read 2|6|8   2-point linear (levels sized 32 per period of the top harmonic, 2048 cap), 6-point
 *                  Lagrange (N = clamp(16 h, 2048, 8192)), 8-point (cap 4096); --maxn overrides the cap.
 *   --spacing 1|2  one level per octave, or half-octave (levels at H*2^(-k/2); the pitch crossfade
 *                  keeps the safe pair (kb, kb+1), linear in log pitch, not WT-5's raised cosine on the
 *                  top third of each range: cost-equal, the per-sample mix weight WT-5 asks for is not
 *                  costed because the workload's pitches are fixed).
 *   --norm 0|1     fixed 0.85, or WT-16: Parseval RMS gain per level (0.85/sqrt2 target), +24 dB cap,
 *                  and the peak limit on the top level (which is then always built).
 *   --dc 0|1       one-pole 8 Hz blocker per voice output (WT-25's fallback; for cost).
 *   --pv 1         per-voice frame under Linear, WT-23 strategy (a): pre-built level sets for every
 *                  corner frame, no rebuild ever, 8 reads per member-sample (16 across the pitch pair);
 *                  XY by a 5 Hz LFO, each voice's frame by a 0.3 Hz LFO at its own phase. Weights update
 *                  per 16-sample tick; the Gram-matrix gain of WT-16 is not costed (fixed 0.85).
 *   --xfade 0|1|2  WT-15 (i) each read computes its own coefficients (a compiler barrier stops CSE for
 *                  the Lagrange reads; the 2-point read has none to recompute), (ii) fused: both sets at
 *                  one index sharing the coefficients, (iii) the interpolated set written every
 *                  --xstep samples (32: four steps per 128) and members read one set.
 * `--followup` runs the follow-up table; `--timed --followup` times those cells (see the end of file).
 *
 * WORKLOAD (horde's PF-3 chord): 8 held notes, MIDI 48 + 3*(k mod 8); 512-sample blocks; 0.25 s
 * warm-up (rounded up to whole blocks) then 2 s measured (likewise). Moving case: X and Y by a 5 Hz
 * sine (90 degrees apart), frame by a 0.3 Hz one; static case: X, Y, frame fixed, rebuild count in
 * the measured window must be zero (asserted).
 *
 * THE MEASURE is INSTRUCTIONS RETIRED, the load-free measure the clean-room REPORT uses
 * (/usr/bin/time -l, whole process). Per cell the tool runs ITSELF twice as child processes
 * (`--cell`): once with the measured window (--seconds 2) and once with none (--seconds 0, a
 * "no-render" baseline that still does start-up, table generation and the warm-up). The measured
 * figure is the difference, so start-up, table generation and warm-up do not count. NO WALL-CLOCK
 * RUN HAPPENS HERE: by ADR-380 A1 timed runs happen only in the 2-6 am quiet window. `--timed` is
 * built and UNRUN (thread CPU time, best of 5); the CPU-% column of the table is an ESTIMATE from
 * instructions at an assumed instructions-per-second (--ips, default 10e9 = 3.2 GHz at IPC ~3.1; a
 * second column assumes half that), not a measurement.
 *
 * SELF-CHECKS RUN FIRST (a number from a wrong implementation is worthless): IFFT against a direct
 * sum, corner identity, Linear equivalence to the bilinear time-domain crossfade, band-limit for MIDI
 * 0-127 at 44.1 and 48 kHz, static patch, determinism - each with a negative CONTROL that must trip.
 * Exit code non-zero on any FAIL.
 *
 * BUILD (standalone, from the repo root; never through the plugin's CMake):
 *   nice -n 10 c++ -std=c++20 -O3 -o "$TMPDIR/qcb" tools/quadrant_cost_bench.cpp
 * RUN:
 *   "$TMPDIR/qcb"                 # self-checks, then the full cell table and the cost breakdown
 *   "$TMPDIR/qcb" --checks        # self-checks only
 *   "$TMPDIR/qcb" --followup      # self-checks, then the follow-up cells and two breakdowns
 *   "$TMPDIR/qcb" --table --maxn 32768   # whole first-run table at the 32-per-period level sizes
 *   "$TMPDIR/qcb" --timed         # QUIET WINDOW ONLY (2-6 am): best-of-5 thread-CPU % per cell (first-run matrix)
 *   "$TMPDIR/qcb" --timed --followup   # the same, for the follow-up cells (+ a 44.1 kHz one)
 *   "$TMPDIR/qcb" --timed --read 6 --norm 1 --dc 1 --law 0 --M 9 --moving 1   # one cell, any option
 *   "$TMPDIR/qcb" --cell --H 1024 --law 1 --M 9 --moving 1 --fs 48000 --seconds 2   # one process
 * A MEASUREMENT, NOT A CHECK: it is not run by ./verify (it needs a C++ compile and a minutes-long
 * run).
 */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace {

constexpr double PI = 3.141592653589793;
constexpr int NF = 16;      // frames per table
constexpr int TICK = 16;    // coupling tick, samples (horde's control tick)
constexpr int CTRL = 128;   // rebuild boundary, samples
constexpr int BLOCK = 512;
constexpr int NCORNER = 4;
constexpr int MAXM = 9;
constexpr int NVOICE = 8;
constexpr float DETUNE = 0.006f;   // +- relative member detune
constexpr float KURA = 0.2f;       // coupling strength in the cost bench

/* mulberry32, bit-exact with the lab's and with tools/filter_cost_bench.cpp. */
struct Mulberry32 {
  uint32_t a;
  explicit Mulberry32(uint32_t s) : a(s) {}
  double operator()() {
    a += 0x6D2B79F5u;
    uint32_t t = (a ^ (a >> 15)) * (1u | a);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return double(t ^ (t >> 14)) / 4294967296.0;
  }
};

struct Cpx { float r, i; };

/* ------------------------------------------------------------------ FFT ----------------------- */
/* Radix-2 complex FFT with +i exponent, unnormalised, plus a real-output IFFT built on a half-size
   complex one. Twiddles T[j] = e^{+2 pi i j / maxN}, j < maxN/2, shared by every size. */
class Fft {
 public:
  explicit Fft(int maxN) : maxN_(maxN), tw_(maxN / 2), scratch_(maxN / 2) {
    for (int j = 0; j < maxN / 2; ++j)
      tw_[j] = {float(std::cos(2.0 * PI * j / maxN)), float(std::sin(2.0 * PI * j / maxN))};
    for (int lg = 1; (1 << lg) <= maxN / 2; ++lg) {
      int n = 1 << lg;
      rev_.emplace_back(n);
      auto& r = rev_.back();
      for (int i = 0; i < n; ++i) {
        uint32_t x = i, y = 0;
        for (int b = 0; b < lg; ++b) { y = (y << 1) | (x & 1); x >>= 1; }
        r[i] = y;
      }
    }
  }
  int maxN() const { return maxN_; }

  void inverseComplex(Cpx* a, int n) const {
    int lg = 0;
    while ((1 << lg) < n) ++lg;
    const auto& rev = rev_[lg - 1];
    for (int i = 0; i < n; ++i) {
      int j = rev[i];
      if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
      int half = len >> 1, step = maxN_ / len;
      for (int i = 0; i < n; i += len)
        for (int k = 0; k < half; ++k) {
          Cpx w = tw_[k * step], u = a[i + k], v = a[i + k + half];
          Cpx t{v.r * w.r - v.i * w.i, v.r * w.i + v.i * w.r};
          a[i + k] = {u.r + t.r, u.i + t.i};
          a[i + k + half] = {u.r - t.r, u.i - t.i};
        }
    }
  }

  /* out[j] = g * sum_{n=1..h} re[n] cos(2 pi n j/N) + im[n] sin(2 pi n j/N), j < N, plus a guard
     out[N] = out[0]. h <= N/2-1. Derivation: x[2m] + i x[2m+1] = IDFT_{N/2}(Z), Z[k] = E[k] + i O[k],
     E = (X[k] + conj X[M-k])/2, O = (X[k] - conj X[M-k]) e^{+2 pi i k/N}/2, X[n] = g (re - i im). */
  void realFromHarmonics(const float* re, const float* im, int h, int N, float g, float* out) const {
    const int M = N / 2, step = maxN_ / N;
    Cpx* z = scratch_.data();
    for (int k = 0; k < M; ++k) {
      Cpx xk{0, 0}, cj{0, 0};
      if (k >= 1 && k <= h) xk = {g * re[k], -g * im[k]};
      int j = M - k;
      if (j >= 1 && j <= h) cj = {g * re[j], g * im[j]};   // conj of X[j]
      Cpx e{0.5f * (xk.r + cj.r), 0.5f * (xk.i + cj.i)};
      Cpx d{0.5f * (xk.r - cj.r), 0.5f * (xk.i - cj.i)};
      Cpx w = tw_[k * step];
      Cpx o{d.r * w.r - d.i * w.i, d.r * w.i + d.i * w.r};
      z[k] = {e.r - o.i, e.i + o.r};
    }
    inverseComplex(z, M);
    for (int m = 0; m < M; ++m) { out[2 * m] = z[m].r; out[2 * m + 1] = z[m].i; }
    out[N] = out[0];
  }

 private:
  int maxN_;
  std::vector<Cpx> tw_;
  mutable std::vector<Cpx> scratch_;
  std::vector<std::vector<uint32_t>> rev_;
};

/* ----------------------------------------------------------------- tables --------------------- */
struct Table {
  int H = 0;
  std::vector<float> re, im, lm;   // [NF][H+1]; lm = ln(|c| + 1e-4), the spec's cache
  const float* R(int f) const { return &re[size_t(f) * (H + 1)]; }
  const float* I(int f) const { return &im[size_t(f) * (H + 1)]; }
  const float* L(int f) const { return &lm[size_t(f) * (H + 1)]; }
};

constexpr float EPS_LOG = 1e-4f;

enum Law { LINEAR = 0, SPECLOG = 1 };

/* Four recipes with rich spectra at every H: sine->saw, pulse-width sweep, vowel-ish formants,
   sync-like comb. Frame peak-normalised to 1.0 through the top-size IFFT, as the spec's data model. */
Table makeTable(int kind, int H, int topN, const Fft& fft, uint32_t seed) {
  Table t;
  t.H = H;
  t.re.assign(size_t(NF) * (H + 1), 0.f);
  t.im = t.re;
  t.lm = t.re;
  Mulberry32 rng(seed + uint32_t(kind) * 7919u);
  std::vector<float> cyc(topN + 1);
  const int hTop = std::min(H, topN / 2 - 1);
  for (int f = 0; f < NF; ++f) {
    const double s = f / double(NF - 1);
    float* re = &t.re[size_t(f) * (H + 1)];
    float* im = &t.im[size_t(f) * (H + 1)];
    for (int n = 1; n <= H; ++n) {
      double a = 0, ph = 0;
      switch (kind) {
        case 0: a = (n == 1) ? 1.0 : s / n; ph = PI / 2; break;                       // sine -> saw
        case 1: { double d = 0.05 + 0.45 * s;                                          // pulse width
                  a = 2.0 / (PI * n) * std::sin(PI * n * d); ph = PI * n * d; break; }
        case 2: { double c1 = H * (0.02 + 0.01 * s), c2 = H * (0.06 - 0.02 * s), c3 = H * 0.12;  // vowel
                  auto g = [&](double c) { double w = 0.4 * c + 1.0, x = (n - c) / w; return std::exp(-x * x); };
                  a = (0.05 + g(c1) + 0.7 * g(c2) + 0.4 * g(c3)) / std::sqrt(double(n));
                  ph = 2 * PI * rng(); break; }
        default: { double r = 1.0 + 5.0 * s;                                           // sync-like comb
                   a = (0.3 + 0.7 * std::fabs(std::sin(PI * n * r / 7.0))) / n; ph = PI * n * s; break; }
      }
      re[n] = float(a * std::cos(ph));
      im[n] = float(a * std::sin(ph));
    }
    fft.realFromHarmonics(re, im, hTop, topN, 1.f, cyc.data());
    float pk = 0;
    for (int j = 0; j < topN; ++j) pk = std::max(pk, std::fabs(cyc[j]));
    const float inv = pk > 1e-9f ? 1.f / pk : 1.f;
    float* lm = &t.lm[size_t(f) * (H + 1)];
    for (int n = 1; n <= H; ++n) {
      re[n] *= inv;
      im[n] *= inv;
      lm[n] = std::log(std::hypot(re[n], im[n]) + EPS_LOG);
    }
  }
  return t;
}

/* The spec's Blend math over harmonics 1..H. Frame lerp first, per corner; the log law lerps the
   cached log-magnitudes (so it is NOT ln|lerp c| at fractional frames: see the corner-identity note
   in the self-checks). */
void blendFrames(const Table* const t[NCORNER], const float w[NCORNER], float framePos, int law, int H,
                 float* re, float* im) {
  const float u = framePos * float(NF - 1);
  const int i0 = std::min(NF - 2, int(u));
  const float fr = u - float(i0);
  const float *ra[4], *rb[4], *ia[4], *ib[4], *la[4], *lb[4];
  int heavy = 0;
  for (int c = 0; c < NCORNER; ++c) {
    ra[c] = t[c]->R(i0); rb[c] = t[c]->R(i0 + 1);
    ia[c] = t[c]->I(i0); ib[c] = t[c]->I(i0 + 1);
    la[c] = t[c]->L(i0); lb[c] = t[c]->L(i0 + 1);
    if (w[c] > w[heavy]) heavy = c;
  }
  for (int n = 1; n <= H; ++n) {
    float sr = 0, si = 0, L = 0, hr = 0, hi = 0;
    for (int c = 0; c < NCORNER; ++c) {
      float r = ra[c][n] + (rb[c][n] - ra[c][n]) * fr;
      float i = ia[c][n] + (ib[c][n] - ia[c][n]) * fr;
      sr += w[c] * r;
      si += w[c] * i;
      if (law == SPECLOG) {
        L += w[c] * (la[c][n] + (lb[c][n] - la[c][n]) * fr);
        if (c == heavy) { hr = r; hi = i; }
      }
    }
    if (law == LINEAR) { re[n] = sr; im[n] = si; continue; }
    const float mag = std::max(0.f, std::exp(L) - EPS_LOG);
    float m = std::sqrt(sr * sr + si * si);
    float cr, ci;
    if (m > 1e-6f) { cr = sr / m; ci = si / m; }
    else {
      float hm = std::sqrt(hr * hr + hi * hi);
      if (hm > 0.f) { cr = hr / hm; ci = hi / hm; } else { cr = 1.f; ci = 0.f; }   // atan2(0,0) = 0
    }
    re[n] = mag * cr;
    im[n] = mag * ci;
  }
}

inline void weightsXY(float x, float y, float w[NCORNER]) {
  w[0] = (1 - x) * (1 - y); w[1] = x * (1 - y); w[2] = (1 - x) * y; w[3] = x * y;
}

/* -------------------------------------------------------------------- options ---------------- */
enum Ablate : unsigned { AB_COUPLING = 1, AB_READ = 2, AB_BLEND = 4, AB_IFFT = 8 };

/* Every run-time option. The defaults reproduce the first run's design (2-point read, 2048 cap, one
   level per octave, fixed 0.85 gain, no DC filter, per-set crossfade of scheme (i)). */
struct Cfg {
  int H = 1024, law = SPECLOG, M = 9, fs = 48000;
  int maxN = 0;      // level-size cap; 0 = by read: 2048 (2-point), 8192 (6-point), 4096 (8-point)
  int read = 2;      // interpolation points: 2 (linear), 6 or 8 (Lagrange; WT-6 / WT-6a)
  int spacing = 1;   // levels per octave: 1, or 2 (half-octave; WT-5)
  int norm = 0;      // 0 fixed 0.85; 1 WT-16 (Parseval RMS gain, +24 dB cap, peak limit)
  int dc = 0;        // 1: one-pole 8 Hz DC blocker per voice output (WT-25's fallback, for cost)
  int pv = 0;        // 1: per-voice frame under Linear, WT-23 strategy (a): 8 reads per member-sample
  int xf = 0;        // rebuild crossfade: 0 scheme (i); 1 (ii) fused (coefficients shared); 2 (iii)
  int xstep = 32;    // scheme (iii): samples between interpolated-set writes (128/32 = 4 steps)
  bool moving = true, hash = false;
  double seconds = 2.0;
  uint32_t seed = 0x51AD0u;
  unsigned ablate = 0;
};

int capFor(const Cfg& c) { return c.maxN ? c.maxN : (c.read == 2 ? 2048 : c.read == 6 ? 8192 : 4096); }

/* ------------------------------------------------------------- level geometry ----------------- */
/* Levels k = 0..: top harmonic h_k = floor(H * 2^(-k/spacing)), down to 1. Size: the 2-point read
   keeps the first run's rule (32 samples per period of the top harmonic, 64 floor); the Lagrange
   reads use WT-6's N = clamp(16 h, 2048, cap). Harmonics are capped at N/2-1. */
struct Geometry {
  int H = 0, nLev = 0, maxN = 0, spacing = 1;
  std::vector<int> N, hs;
  Geometry(int H_, int spacing_, int read, int cap) : H(H_), spacing(spacing_) {
    for (int k = 0;; ++k) {
      int h = std::max(1, int(std::floor(H * std::exp2(-double(k) / spacing) + 1e-9)));
      int n;
      if (read == 2) { n = 64; while (n < 32 * h) n <<= 1; }
      else { n = 2048; while (n < 16 * h) n <<= 1; }
      n = std::min(n, cap);
      N.push_back(n);
      hs.push_back(std::min(h, n / 2 - 1));
      if (h == 1) break;
    }
    nLev = int(N.size());
    maxN = *std::max_element(N.begin(), N.end());
  }
};

struct LevSel { int la, lb; float wb; };

/* Two adjacent levels by pitch; both at or below the band limit (see the file header). pos counts
   level steps, so half-octave spacing doubles it. */
LevSel selectLevels(float f, int fs, int H, int nLev, int spacing) {
  const float lim = std::min(20000.f, 0.45f * float(fs));
  const float pos = std::log2(float(H) * f / lim) * float(spacing);
  const float fl = std::floor(pos);
  const int kb = int(fl) + 1;
  LevSel s;
  s.la = std::clamp(kb, 0, nLev - 1);
  s.lb = std::clamp(kb + 1, 0, nLev - 1);
  s.wb = (s.la == s.lb) ? 0.f : pos - fl;
  return s;
}

inline float midiHz(int key) { return 440.f * std::exp2(float(key - 69) / 12.f); }

/* ------------------------------------------------------------------ the read ----------------- */
/* P-point Lagrange read between samples i and i+1 (nodes lo..lo+P-1). P = 2 is the first run's lerp
   expression unchanged. Levels carry 4 wrapped guard samples each side so no index wraps mid-kernel. */
constexpr int GUARD = 4;

template <int P>
struct Rd {
  static constexpr int lo = -(P / 2 - 1);
  static constexpr std::array<float, P> K = [] {
    std::array<float, P> k{};
    for (int a = 0; a < P; ++a) {
      double d = 1;
      for (int b = 0; b < P; ++b) if (b != a) d *= double(a - b);
      k[a] = float(1.0 / d);
    }
    return k;
  }();
  int i;
  float f;
  float w[P];
  inline void prep(float q) {
    i = int(q);
    f = q - float(i);
    if constexpr (P > 2) {
      float pre[P + 1], suf[P + 1];
      pre[0] = 1; suf[P] = 1;
      float d[P];
      for (int k = 0; k < P; ++k) d[k] = f - float(lo + k);
      for (int k = 0; k < P; ++k) pre[k + 1] = pre[k] * d[k];
      for (int k = P - 1; k >= 0; --k) suf[k] = suf[k + 1] * d[k];
      for (int k = 0; k < P; ++k) w[k] = K[k] * pre[k] * suf[k + 1];
    }
  }
  inline float at(const float* a) const {
    if constexpr (P == 2) return a[i] + (a[i + 1] - a[i]) * f;
    else {
      const float* p = a + i + lo;
      float y = 0;
      for (int k = 0; k < P; ++k) y += p[k] * w[k];
      return y;
    }
  }
};

void fillGuards(float* p, int N) {   // p = level data pointer (already offset by GUARD)
  for (int j = 0; j < GUARD; ++j) { p[N + j] = p[j]; p[-1 - j] = p[N - 1 - j]; }
}

/* ------------------------------------------------------------------ the cell ----------------- */
struct Result { uint64_t rebuilds = 0, totalRebuilds = 0; uint32_t hash = 0; double windowSec = 0; uint64_t peakEng = 0, capEng = 0; };

struct Set {
  std::vector<std::vector<float>> lvl;   // N + 2*GUARD each; data starts at GUARD
  std::vector<char> has;
  float* p(int k) { return lvl[k].data() + GUARD; }
  const float* p(int k) const { return lvl[k].data() + GUARD; }
};

class Sim {
 public:
  Sim(const Cfg& c, const Fft& fft, const Table* const tabs[NCORNER], const Geometry& g)
      : c_(c), fft_(fft), g_(g), out_(BLOCK) {
    for (int k = 0; k < NCORNER; ++k) tab_[k] = tabs[k];
    auto alloc = [&](Set& s) {
      s.lvl.resize(g.nLev);
      for (int k = 0; k < g.nLev; ++k) s.lvl[k].assign(g.N[k] + 2 * GUARD, 0.f);
      s.has.assign(g.nLev, 0);
    };
    for (auto& s : set_) alloc(s);
    alloc(mix_);
    bre_.assign(g.H + 1, 0.f);
    bim_.assign(g.H + 1, 0.f);
    en_.assign(g.H + 1, 0.f);
    Mulberry32 rng(c.seed);
    for (int v = 0; v < NVOICE; ++v) {
      Voice& vo = v_[v];
      vo.f = midiHz(48 + 3 * (v % 8));
      vo.frameOff = 0.7f * float(v);
      vo.ls = selectLevels(vo.f * (1.f + DETUNE), c.fs, g.H, g.nLev, g.spacing);
      for (int m = 0; m < c.M; ++m) {
        float rel = c.M > 1 ? ((float(m) + 0.5f) / float(c.M) - 0.5f) * 2.f * DETUNE : 0.f;
        vo.base[m] = vo.f * (1.f + rel) / float(c.fs);
        vo.inc[m] = vo.base[m];
        vo.ph[m] = float(rng());
      }
    }
    dcR_ = 1.f - float(2 * PI * 8.0 / c.fs);
    if (c.pv) buildPerCorner();   // off the audio thread in a real engine: counted in the baseline process
  }

  /* One 512-sample block; every 128 a control step, every 16 a coupling step. */
  void block() {
    std::fill(out_.begin(), out_.end(), 0.f);
    for (int tk = 0; tk < BLOCK / TICK; ++tk, pos_ += TICK) {
      if (!c_.pv && pos_ % CTRL == 0) control();
      if (c_.xf == 2 && fading_ && pos_ % uint64_t(c_.xstep) == 0) mixStep();
      switch (c_.read) {
        case 6: tick<6>(&out_[tk * TICK]); break;
        case 8: tick<8>(&out_[tk * TICK]); break;
        default: tick<2>(&out_[tk * TICK]); break;
      }
    }
    if (c_.hash) {
      uint32_t h = hash_;
      for (float f : out_) { uint32_t b; std::memcpy(&b, &f, 4); h = (h ^ b) * 16777619u; }
      hash_ = h;
    }
    asm volatile("" ::"r"(out_.data()) : "memory");   // the render cannot be dead-code-eliminated
  }

  uint64_t rebuilds() const { return rebuilds_; }
  uint32_t hash() const { return hash_; }
  uint64_t peakEng() const { return peakEng_; }
  uint64_t capEng() const { return capEng_; }
  const Set& cur() const { return set_[cur_]; }
  const Set& mix() const { return mix_; }
  const float* pvCycle(int corner, int frame, int k) const { return pvc_[(corner * NF + frame) * g_.nLev + k].data() + GUARD; }
  /* For the self-checks: build the set for (x, y, frame) now. */
  void buildNow(float x, float y, float frame, unsigned mask) { rebuild(x, y, frame, mask); }
  unsigned need() const { unsigned m = 0; for (auto& v : v_) m |= (1u << v.ls.la) | (1u << v.ls.lb); return m; }

 private:
  struct Voice {
    float f = 0, base[MAXM], inc[MAXM], ph[MAXM], frameOff = 0, dcx = 0, dcy = 0;
    LevSel ls{0, 0, 0};
  };

  void control() {
    float x = 0.35f, y = 0.3f, frame = 0.4f;
    if (c_.moving) {
      const double t = double(pos_) / c_.fs;
      x = float(0.5 + 0.5 * std::sin(2 * PI * 5.0 * t));
      y = float(0.5 + 0.5 * std::cos(2 * PI * 5.0 * t));
      frame = float(0.5 + 0.5 * std::sin(2 * PI * 0.3 * t));
    }
    unsigned nd = 0;
    for (auto& vo : v_) {
      vo.ls = selectLevels(vo.f * (1.f + DETUNE), c_.fs, g_.H, g_.nLev, g_.spacing);
      nd |= 1u << vo.ls.la;
      nd |= 1u << vo.ls.lb;
    }
    const bool changed = !built_ || x != lx_ || y != ly_ || frame != lf_ || (nd & ~builtMask_);
    if (changed) rebuild(x, y, frame, nd);
  }

  void buildLevel(Set& s, int k, float gain) {
    fft_.realFromHarmonics(bre_.data(), bim_.data(), g_.hs[k], g_.N[k], gain, s.p(k));
    fillGuards(s.p(k), g_.N[k]);
  }

  void rebuild(float x, float y, float frame, unsigned mask) {
    float w[NCORNER];
    weightsXY(x, y, w);
    if (!(c_.ablate & AB_BLEND) || !built_) blendFrames(tab_, w, frame, c_.law, g_.H, bre_.data(), bim_.data());
    const int nxt = built_ ? 1 - cur_ : 0;
    Set& s = set_[nxt];
    if (c_.norm) mask |= 1u;   // WT-16's peak limit reads the top level's cycle
    for (int k = 0; k < g_.nLev; ++k) s.has[k] = (mask >> k) & 1u;
    const bool doFft = !(c_.ablate & AB_IFFT) || !prevBuilt_;
    if (!c_.norm) {
      for (int k = 0; k < g_.nLev; ++k)
        if (s.has[k] && doFft) buildLevel(s, k, 0.85f);
    } else if (doFft) {
      // WT-16: Parseval RMS gain per level, +24 dB cap, then the top level's peak limit.
      float acc = 0;
      for (int n = 1; n <= g_.H; ++n) { acc += bre_[n] * bre_[n] + bim_[n] * bim_[n]; en_[n] = acc; }
      auto gainFor = [&](int k) {
        float rms = std::sqrt(0.5f * en_[g_.hs[k]]);
        float gn = rms > 1e-9f ? (0.85f * 0.70710678f) / rms : 1e9f;
        if (gn > 15.848932f) { gn = 15.848932f; ++capEng_; }
        return gn;
      };
      float g0 = gainFor(0);
      buildLevel(s, 0, g0);
      float pk = 0;
      const float* p0 = s.p(0);
      for (int j = 0; j < g_.N[0]; ++j) pk = std::max(pk, std::fabs(p0[j]));
      float lim = 1.f;
      if (pk > 1.f) {
        lim = 1.f / pk;
        ++peakEng_;
        float* q = s.p(0);
        for (int j = 0; j < g_.N[0]; ++j) q[j] *= lim;
        fillGuards(q, g_.N[0]);
      }
      for (int k = 1; k < g_.nLev; ++k)
        if (s.has[k]) buildLevel(s, k, gainFor(k) * lim);
    }
    prevBuilt_ = true;
    if (c_.xf == 2) {
      if (!built_) { copySet(mix_, s, mask); fading_ = false; }
      else { fading_ = true; fadeStart_ = pos_; }
    } else if (built_) { fading_ = true; fadeX_ = 0.f; }
    else fading_ = false;
    if (built_) prev_ = cur_;
    cur_ = nxt;
    built_ = true;
    builtMask_ = mask;
    lx_ = x; ly_ = y; lf_ = frame;
    ++rebuilds_;
  }

  void copySet(Set& dst, const Set& src, unsigned mask) {
    for (int k = 0; k < g_.nLev; ++k) {
      dst.has[k] = (mask >> k) & 1u;
      if (dst.has[k]) std::memcpy(dst.lvl[k].data(), src.lvl[k].data(), src.lvl[k].size() * sizeof(float));
    }
  }

  /* Scheme (iii): write (1-g)*old + g*new once per xstep for the levels in use; members read mix_ only. */
  void mixStep() {
    const float g = float((pos_ - fadeStart_) / uint64_t(c_.xstep) + 1) * float(c_.xstep) / float(CTRL);
    const Set& o = set_[prev_];
    const Set& n = set_[cur_];
    const float gg = std::min(1.f, g);
    for (int k = 0; k < g_.nLev; ++k) {
      if (!n.has[k]) continue;
      mix_.has[k] = 1;
      float* d = mix_.lvl[k].data();
      const float* nv = n.lvl[k].data();
      if (o.has[k]) {
        const float* ov = o.lvl[k].data();
        const size_t sz = n.lvl[k].size();
        for (size_t j = 0; j < sz; ++j) d[j] = ov[j] + (nv[j] - ov[j]) * gg;
      } else std::memcpy(d, nv, n.lvl[k].size() * sizeof(float));
    }
    if (g >= 1.f) fading_ = false;
  }

  /* WT-23 (a): level sets for every corner frame, built once. Fixed 0.85 gain (the Gram-matrix gain
     of WT-16 is per control step and not costed here). */
  void buildPerCorner() {
    const unsigned nd = need();
    pvc_.resize(size_t(NCORNER) * NF * g_.nLev);
    for (int c = 0; c < NCORNER; ++c)
      for (int f = 0; f < NF; ++f)
        for (int k = 0; k < g_.nLev; ++k) {
          auto& v = pvc_[(c * NF + f) * g_.nLev + k];
          if (!((nd >> k) & 1u)) continue;
          v.assign(g_.N[k] + 2 * GUARD, 0.f);
          fft_.realFromHarmonics(tab_[c]->R(f), tab_[c]->I(f), g_.hs[k], g_.N[k], 0.85f, v.data() + GUARD);
          fillGuards(v.data() + GUARD, g_.N[k]);
        }
  }

  template <int P>
  void tick(float* out) {
    const int M = c_.M;
    float xw[TICK];
    const bool twoSet = c_.xf != 2 && !c_.pv;
    if (twoSet && fading_) {
      for (int s = 0; s < TICK; ++s) xw[s] = std::min(1.f, fadeX_ + float(s + 1) * (1.f / CTRL));
      fadeX_ = xw[TICK - 1];
    }
    // per-voice-frame state (strategy (a)): weights from the shared XY, frame from each voice
    float wxy[NCORNER] = {0, 0, 0, 0};
    if (c_.pv) {
      float x = 0.35f, y = 0.3f;
      if (c_.moving) {
        const double t = double(pos_) / c_.fs;
        x = float(0.5 + 0.5 * std::sin(2 * PI * 5.0 * t));
        y = float(0.5 + 0.5 * std::cos(2 * PI * 5.0 * t));
      }
      weightsXY(x, y, wxy);
    }
    const Set& cs = (c_.xf == 2) ? mix_ : set_[cur_];
    const Set& ps = set_[prev_];
    for (auto& vo : v_) {
      if (!(c_.ablate & AB_COUPLING)) {
        float cs_[MAXM], sn_[MAXM], zr = 0, zi = 0;
        for (int m = 0; m < M; ++m) {
          float th = 6.2831855f * vo.ph[m];
          cs_[m] = std::cos(th); sn_[m] = std::sin(th);
          zr += cs_[m]; zi += sn_[m];
        }
        zr /= float(M); zi /= float(M);
        for (int m = 0; m < M; ++m) vo.inc[m] = vo.base[m] * (1.f + KURA * (zi * cs_[m] - zr * sn_[m]));
      }
      const LevSel ls = vo.ls;
      const bool two = ls.la != ls.lb;
      const float wb = ls.wb, fNa = float(g_.N[ls.la]), fNb = float(g_.N[ls.lb]);
      const bool noread = c_.ablate & AB_READ;
      float vbuf[TICK];
      if (c_.dc) for (int s = 0; s < TICK; ++s) vbuf[s] = 0.f;
      float* const vacc = c_.dc ? vbuf : out;   // without the DC filter members add straight into the block, as the first run did

      if (c_.pv) {
        float k0[NCORNER], k1[NCORNER];
        const float* ca0[NCORNER]; const float* ca1[NCORNER]; const float* cb0[NCORNER]; const float* cb1[NCORNER];
        {
          float fp = 0.5f, fr;
          if (c_.moving) fp = float(0.5 + 0.5 * std::sin(2 * PI * 0.3 * double(pos_) / c_.fs + vo.frameOff));
          const float u = fp * float(NF - 1);
          const int i0 = std::min(NF - 2, int(u));
          fr = u - float(i0);
          for (int c = 0; c < NCORNER; ++c) {
            k0[c] = wxy[c] * (1.f - fr); k1[c] = wxy[c] * fr;
            ca0[c] = pvCycle(c, i0, ls.la); ca1[c] = pvCycle(c, i0 + 1, ls.la);
            cb0[c] = pvCycle(c, i0, ls.lb); cb1[c] = pvCycle(c, i0 + 1, ls.lb);
          }
        }
        for (int m = 0; m < M; ++m) {
          float ph = vo.ph[m];
          const float inc = vo.inc[m];
          for (int s = 0; s < TICK; ++s) {
            float y = 0.f;
            if (!noread) {
              Rd<P> ra, rb;
              ra.prep(ph * fNa);
              float sa = 0;
              for (int c = 0; c < NCORNER; ++c) sa += k0[c] * ra.at(ca0[c]) + k1[c] * ra.at(ca1[c]);
              y = sa;
              if (two) {
                rb.prep(ph * fNb);
                float sb = 0;
                for (int c = 0; c < NCORNER; ++c) sb += k0[c] * rb.at(cb0[c]) + k1[c] * rb.at(cb1[c]);
                y += (sb - y) * wb;
              }
            }
            vacc[s] += y;
            ph += inc;
            ph -= (ph >= 1.f) ? 1.f : 0.f;
          }
          vo.ph[m] = ph;
        }
      } else {
        const float *A = cs.p(ls.la), *B = cs.p(ls.lb);
        const bool fade = twoSet && fading_ && ps.has[ls.la] && ps.has[ls.lb];
        const float *PA = ps.p(ls.la), *PB = ps.p(ls.lb);
        const bool fused = c_.xf == 1 || P == 2;   // P = 2 has no coefficients to recompute: the first run's shared form
        for (int m = 0; m < M; ++m) {
          float ph = vo.ph[m];
          const float inc = vo.inc[m];
          for (int s = 0; s < TICK; ++s) {
            float y = 0.f;
            if (!noread) {
              const float qa = ph * fNa, qb = ph * fNb;
              Rd<P> ra, rb;
              ra.prep(qa);
              y = ra.at(A);
              if (two) { rb.prep(qb); y += (rb.at(B) - y) * wb; }
              if (fade) {
                float u;
                if (fused) {   // scheme (ii): the old set at the same index, coefficients shared
                  u = ra.at(PA);
                  if (two) u += (rb.at(PB) - u) * wb;
                } else {       // scheme (i): each read computes its own coefficients (the barrier stops CSE)
                  float qa2 = qa, qb2 = qb;
                  asm volatile("" : "+r"(qa2), "+r"(qb2));
                  Rd<P> pa, pb;
                  pa.prep(qa2);
                  u = pa.at(PA);
                  if (two) { pb.prep(qb2); u += (pb.at(PB) - u) * wb; }
                }
                y = u + (y - u) * xw[s];
              }
            }
            vacc[s] += y;
            ph += inc;
            ph -= (ph >= 1.f) ? 1.f : 0.f;
          }
          vo.ph[m] = ph;
        }
      }
      if (c_.dc) {
        for (int s = 0; s < TICK; ++s) {
          const float yy = vbuf[s] - vo.dcx + dcR_ * vo.dcy;
          vo.dcx = vbuf[s];
          vo.dcy = yy;
          out[s] += yy;
        }
      }
    }
    if (twoSet && fading_ && fadeX_ >= 1.f) fading_ = false;
  }

  Cfg c_;
  const Fft& fft_;
  Geometry g_;
  const Table* tab_[NCORNER];
  Set set_[2], mix_;
  std::vector<std::vector<float>> pvc_;
  int cur_ = 0, prev_ = 1;
  bool built_ = false, prevBuilt_ = false, fading_ = false;
  float fadeX_ = 1.f, lx_ = 0, ly_ = 0, lf_ = 0, dcR_ = 1.f;
  unsigned builtMask_ = 0;
  uint64_t rebuilds_ = 0, pos_ = 0, fadeStart_ = 0, peakEng_ = 0, capEng_ = 0;
  uint32_t hash_ = 2166136261u;
  std::vector<float> bre_, bim_, en_, out_;
  Voice v_[NVOICE];
};

struct World {
  Geometry g;
  Fft fft;
  Table t[NCORNER];
  const Table* tp[NCORNER];
  explicit World(const Cfg& c) : g(c.H, c.spacing, c.read, capFor(c)), fft(g.maxN) {
    for (int k = 0; k < NCORNER; ++k) { t[k] = makeTable(k, c.H, g.N[0], fft, 1234u); tp[k] = &t[k]; }
  }
};

Result runCell(const Cfg& c) {
  World w(c);
  Sim sim(c, w.fft, w.tp, w.g);
  auto blocksFor = [&](double s) { return int(std::ceil(s * c.fs / BLOCK - 1e-9)); };
  const int warm = blocksFor(0.25), meas = c.seconds > 0 ? blocksFor(c.seconds) : 0;
  for (int b = 0; b < warm; ++b) sim.block();
  const uint64_t r0 = sim.rebuilds();
  for (int b = 0; b < meas; ++b) sim.block();
  Result r;
  r.rebuilds = sim.rebuilds() - r0;
  r.totalRebuilds = sim.rebuilds();
  r.hash = sim.hash();
  r.windowSec = double(meas) * BLOCK / c.fs;
  r.peakEng = sim.peakEng();
  r.capEng = sim.capEng();
  return r;
}

/* ------------------------------------------------------------------ self-checks -------------- */
int g_fail = 0;
void report(const char* name, bool ok, const char* detail) {
  std::printf("  %-4s %s  %s\n", ok ? "PASS" : "FAIL", name, detail);
  if (!ok) ++g_fail;
}

void checkIfft() {
  Mulberry32 rng(99);
  double worst = 0;
  for (int N : {64, 128, 2048}) {
    Fft fft(2048);
    const int h = N / 2 - 1;
    std::vector<float> re(h + 1), im(h + 1), out(N + 1);
    for (int n = 1; n <= h; ++n) { re[n] = float(rng() - 0.5); im[n] = float(rng() - 0.5); }
    fft.realFromHarmonics(re.data(), im.data(), h, N, 0.85f, out.data());
    for (int j = 0; j < N; ++j) {
      double d = 0;
      for (int n = 1; n <= h; ++n) d += re[n] * std::cos(2 * PI * n * j / N) + im[n] * std::sin(2 * PI * n * j / N);
      worst = std::max(worst, std::fabs(0.85 * d - out[j]));
    }
  }
  char buf[96];
  std::snprintf(buf, sizeof buf, "max |IFFT - direct sum| = %.2e (limit 1e-4)", worst);
  report("ifft vs direct sum", worst < 1e-4, buf);
}

/* The read itself: a pure sine of harmonic h read at random phases. 2-point must miss, 6 and 8 hit. */
template <int P>
double readErr(int N, int h) {
  Fft fft(N);
  std::vector<float> re(h + 1, 0.f), im(h + 1, 0.f), buf(N + 2 * GUARD, 0.f);
  im[h] = 1.f;
  fft.realFromHarmonics(re.data(), im.data(), h, N, 1.f, buf.data() + GUARD);
  fillGuards(buf.data() + GUARD, N);
  Mulberry32 rng(5);
  double w = 0;
  for (int t = 0; t < 4000; ++t) {
    float ph = float(rng());
    Rd<P> r;
    r.prep(ph * float(N));
    w = std::max(w, std::fabs(double(r.at(buf.data() + GUARD)) - std::sin(2 * PI * h * double(ph))));
  }
  return w;
}

void checkReads() {
  const double e2 = readErr<2>(2048, 64), e6 = readErr<6>(2048, 64), e8 = readErr<8>(2048, 64);
  char buf[160];
  std::snprintf(buf, sizeof buf, "sine h=64 at N=2048 (32 samples/period): err 2-pt %.1e, 6-pt %.1e, 8-pt %.1e (6 and 8 < 1e-5; 2-pt must exceed 1e-4)", e2, e6, e8);
  report("interpolating reads", e6 < 1e-5 && e8 < 1e-5 && e2 > 1e-4, buf);
}

double relMagErr(const float* re, const float* im, const double* rr, const double* ri, int H) {
  double w = 0;
  for (int n = 1; n <= H; ++n) {
    double a = std::hypot(double(re[n]), double(im[n])), b = std::hypot(rr[n], ri[n]);
    w = std::max(w, std::fabs(a - b) / (1e-3 * b + 1e-6));   // <= 1 means within 1e-3 relative (+1e-6 abs floor)
  }
  return w;
}

void checkCorners(int H) {
  Cfg c; c.H = H;
  World w(c);
  std::vector<float> re(H + 1), im(H + 1);
  std::vector<double> rr(H + 1), ri(H + 1);
  const float cxy[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  auto refFrame = [&](int corner, float pos) {
    float u = pos * float(NF - 1);
    int i0 = std::min(NF - 2, int(u));
    double f = double(u - float(i0));
    for (int n = 1; n <= H; ++n) {
      rr[n] = w.t[corner].R(i0)[n] + (w.t[corner].R(i0 + 1)[n] - w.t[corner].R(i0)[n]) * f;
      ri[n] = w.t[corner].I(i0)[n] + (w.t[corner].I(i0 + 1)[n] - w.t[corner].I(i0)[n]) * f;
    }
  };
  for (int law = 0; law < 2; ++law) {
    for (float pos : {0.f, 0.4f, 1.f, 0.43f}) {
      double worst = 0, ctl = 1e9;
      for (int k = 0; k < 4; ++k) {
        float wt[4];
        weightsXY(cxy[k][0], cxy[k][1], wt);
        blendFrames(w.tp, wt, pos, law, H, re.data(), im.data());
        refFrame(k, pos);
        worst = std::max(worst, relMagErr(re.data(), im.data(), rr.data(), ri.data(), H));
        refFrame((k + 1) % 4, pos);   // control: the WRONG corner's frame must read as a mismatch
        ctl = std::min(ctl, relMagErr(re.data(), im.data(), rr.data(), ri.data(), H));
      }
      char name[64], buf[160];
      std::snprintf(name, sizeof name, "corner identity H=%d %s frame=%.2f", H, law ? "spectral-log" : "linear", pos);
      std::snprintf(buf, sizeof buf, "worst err / tol = %.3g (<=1 passes); control (wrong corner) min = %.3g (must be >1)", worst, ctl);
      /* Spectral log at a FRACTIONAL frame lerps the cached ln|c|, a geometric lerp of magnitudes:
         it cannot equal |lerp c|. Reported as a NOTE (B648 finding), not a FAIL; integer frames
         (u = 0, 6, 15) are the pass condition. 0.4*15 = 6 exactly, so 0.4 is an integer frame. */
      if (law == SPECLOG && pos == 0.43f)
        std::printf("  NOTE %s  %s  [expected by design: cached-log lerp != |lerp c|]\n", name, buf);
      else
        report(name, worst <= 1.0 && ctl > 1.0, buf);
    }
  }
}

void checkLinearEquivalence(int H, int read, int spacing) {
  Cfg c; c.H = H; c.law = LINEAR; c.M = 1; c.moving = false; c.read = read; c.spacing = spacing;
  World w(c);
  const int N = w.g.N[0], hT = w.g.hs[0];
  Sim sim(c, w.fft, w.tp, w.g);
  double worst = 0, ctl = 0;
  struct P { float x, y, f; };
  for (P p : {P{0.35f, 0.3f, 0.4f}, P{0.8f, 0.6f, 0.43f}, P{0.5f, 0.5f, 0.97f}}) {
    sim.buildNow(p.x, p.y, p.f, 1u);
    const float* lvl = sim.cur().p(0);
    float wt[4];
    weightsXY(p.x, p.y, wt);
    std::vector<double> sum(N, 0.0), avg(N, 0.0);
    std::vector<float> re(H + 1), im(H + 1), cyc(N + 1);
    for (int corner = 0; corner < 4; ++corner) {
      float one[4] = {0, 0, 0, 0};
      one[corner] = 1.f;
      blendFrames(w.tp, one, p.f, LINEAR, H, re.data(), im.data());
      w.fft.realFromHarmonics(re.data(), im.data(), hT, N, 0.85f, cyc.data());
      for (int j = 0; j < N; ++j) { sum[j] += double(wt[corner]) * cyc[j]; avg[j] += 0.25 * cyc[j]; }
    }
    for (int j = 0; j < N; ++j) {
      worst = std::max(worst, std::fabs(sum[j] - lvl[j]));
      ctl = std::max(ctl, std::fabs(avg[j] - lvl[j]));
    }
  }
  char buf[160], name[64];
  std::snprintf(buf, sizeof buf, "max |blend - bilinear crossfade of 4 cycles| = %.2e (limit 1e-5); control (unweighted mean) = %.2e (must exceed)", worst, ctl);
  std::snprintf(name, sizeof name, "linear equivalence H=%d N0=%d", H, N);
  report(name, worst < 1e-5 && ctl > 1e-5, buf);
}

/* WT-23 (a): the 8-read blend of pre-built per-corner cycles equals the pre-blend (b) at the same
   (x, y, frame). Same fixed 0.85 gain on both sides. */
void checkPerVoiceBlend() {
  Cfg c; c.H = 1024; c.law = LINEAR; c.M = 1; c.moving = false; c.read = 6; c.pv = 1;
  World w(c);
  Sim sim(c, w.fft, w.tp, w.g);
  Cfg b = c; b.pv = 0;
  Sim ref(b, w.fft, w.tp, w.g);
  const unsigned nd = sim.need();
  double worst = 0, ctl = 0;
  struct P { float x, y, f; };
  for (P p : {P{0.35f, 0.3f, 0.4f}, P{0.8f, 0.6f, 0.43f}, P{0.1f, 0.9f, 0.97f}}) {
    ref.buildNow(p.x, p.y, p.f, nd);
    float wt[4];
    weightsXY(p.x, p.y, wt);
    const float u = p.f * float(NF - 1);
    const int i0 = std::min(NF - 2, int(u));
    const float fr = u - float(i0);
    for (int k = 0; k < w.g.nLev; ++k) {
      if (!((nd >> k) & 1u)) continue;
      const float* r = ref.cur().p(k);
      for (int j = 0; j < w.g.N[k]; ++j) {
        double s = 0;
        for (int cc = 0; cc < 4; ++cc)
          s += double(wt[cc]) * ((1 - fr) * sim.pvCycle(cc, i0, k)[j] + fr * sim.pvCycle(cc, i0 + 1, k)[j]);
        worst = std::max(worst, std::fabs(s - r[j]));
        ctl = std::max(ctl, std::fabs(0.25 * (sim.pvCycle(0, i0, k)[j] + sim.pvCycle(1, i0, k)[j] + sim.pvCycle(2, i0, k)[j] + sim.pvCycle(3, i0, k)[j]) - r[j]));
      }
    }
  }
  char buf[160];
  std::snprintf(buf, sizeof buf, "max |8-read blend - pre-blend| over the levels in use = %.2e (limit 1e-5); control (unweighted) = %.2e (must exceed)", worst, ctl);
  report("per-voice frame blend (a) = pre-blend (b)", worst < 1e-5 && ctl > 1e-5, buf);
}

/* WT-16: the built top level has RMS 0.85/sqrt2 unless the cap or the peak limit engaged; peak <= 1. */
void checkNorm() {
  Cfg c; c.H = 1024; c.law = SPECLOG; c.M = 1; c.moving = false; c.read = 6; c.norm = 1;
  World w(c);
  Sim sim(c, w.fft, w.tp, w.g);
  Cfg f = c; f.norm = 0;
  Sim fixed(f, w.fft, w.tp, w.g);
  int ok = 0, n = 0;
  double worstPk = 0, rmsErr = 0, ctlRms = 0;
  struct P { float x, y, f; };
  for (P p : {P{0.35f, 0.3f, 0.4f}, P{0.8f, 0.6f, 0.43f}, P{0.1f, 0.9f, 0.97f}, P{0.5f, 0.5f, 0.2f}}) {
    sim.buildNow(p.x, p.y, p.f, 1u);
    fixed.buildNow(p.x, p.y, p.f, 1u);
    auto stats = [&](const float* q, double& rms, double& pk) {
      double s = 0; pk = 0;
      for (int j = 0; j < w.g.N[0]; ++j) { s += double(q[j]) * q[j]; pk = std::max(pk, std::fabs(double(q[j]))); }
      rms = std::sqrt(s / w.g.N[0]);
    };
    double rms, pk, rmsF, pkF;
    stats(sim.cur().p(0), rms, pk);
    stats(fixed.cur().p(0), rmsF, pkF);
    const double target = 0.85 * 0.7071067811865476;
    worstPk = std::max(worstPk, pk);
    const bool onTarget = std::fabs(rms - target) < 1e-3 * target, limited = std::fabs(pk - 1.0) < 1e-4;
    rmsErr = std::max(rmsErr, std::min(std::fabs(rms - target) / target, std::fabs(pk - 1.0)));
    ctlRms = std::max(ctlRms, std::fabs(rmsF - target) / target);
    ++n;
    if (pk <= 1.0 + 1e-4 && (onTarget || limited)) ++ok;
  }
  char buf[200];
  std::snprintf(buf, sizeof buf, "%d/%d blends at RMS target or peak-limited, worst peak %.5f; control (fixed 0.85) misses the RMS target by up to %.0f %% (must exceed 1 %%)", ok, n, worstPk, 100 * ctlRms);
  report("WT-16 loudness + peak limit", ok == n && ctlRms > 0.01, buf);
  (void)rmsErr;
}

void checkBandLimit() {
  for (int H : {64, 1024})
    for (int fs : {44100, 48000})
      for (int spacing : {1, 2})
        for (int read : {2, 6, 8}) {
          Geometry g(H, spacing, read, read == 2 ? 2048 : read == 6 ? 8192 : 4096);
          const float lim = std::min(20000.f, 0.45f * float(fs));
          int bad = 0, badCtl = 0;
          float worstTop = 0;
          for (int key = 0; key <= 127; ++key) {
            const float f = midiHz(key) * (1.f + DETUNE);
            LevSel s = selectLevels(f, fs, H, g.nLev, spacing);
            for (int k : {s.la, s.lb}) {
              const float top = float(g.hs[k]) * f;
              worstTop = std::max(worstTop, top / lim);
              if (top > lim * 1.0000001f) ++bad;
            }
            if (float(g.hs[0]) * f > lim * 1.0000001f) ++badCtl;   // control: "always level 0" must trip
          }
          char name[80], buf[160];
          std::snprintf(name, sizeof name, "band-limit H=%d fs=%d spacing=%d read=%d", H, fs, spacing, read);
          std::snprintf(buf, sizeof buf, "levels over the limit = %d; worst top/limit = %.3f; control trips on %d notes", bad, worstTop, badCtl);
          report(name, bad == 0 && (H == 64 ? true : badCtl > 0), buf);
        }
}

void checkXfadeEnd() {
  Cfg c; c.H = 1024; c.M = 1; c.moving = true; c.read = 6; c.xf = 2;
  World w(c);
  Sim sim(c, w.fft, w.tp, w.g);
  for (int b = 0; b < 12; ++b) sim.block();
  /* at a block end the last 32 samples of the 128 are at g = 1: the interpolated set equals the new one */
  double worst = 0;
  const Set& a = sim.cur();
  const Set& m = sim.mix();
  for (int k = 0; k < w.g.nLev; ++k)
    if (a.has[k] && m.has[k])
      for (int j = 0; j < w.g.N[k]; ++j) worst = std::max(worst, std::fabs(double(a.p(k)[j]) - m.p(k)[j]));
  char buf[96];
  std::snprintf(buf, sizeof buf, "max |interpolated set - new set| at the end of the rebuild interval = %.2e (must be 0)", worst);
  report("scheme (iii) lands on the new set", worst == 0, buf);
}

void checkStaticAndDeterminism() {
  for (int H : {64, 1024}) {
    Cfg c; c.H = H; c.M = 3; c.moving = false; c.seconds = 1.0;
    Result r = runCell(c);
    char buf[128];
    std::snprintf(buf, sizeof buf, "rebuilds in the measured window = %llu (must be 0); warm-up+window total = %llu", (unsigned long long)r.rebuilds, (unsigned long long)r.totalRebuilds);
    char name[48];
    std::snprintf(name, sizeof name, "static patch H=%d", H);
    report(name, r.rebuilds == 0, buf);
    Cfg m = c; m.moving = true;
    Result rm = runCell(m);
    std::snprintf(buf, sizeof buf, "control (moving): %llu rebuilds in 1 s (must be >0)", (unsigned long long)rm.rebuilds);
    report("static patch control", rm.rebuilds > 0, buf);
  }
  { /* the new options too: static under 6-point + WT-16 + half-octave + scheme (iii) */
    Cfg c; c.H = 1024; c.M = 3; c.moving = false; c.seconds = 1.0; c.read = 6; c.norm = 1; c.spacing = 2; c.xf = 2; c.dc = 1;
    Result r = runCell(c);
    char buf[96];
    std::snprintf(buf, sizeof buf, "rebuilds in the measured window = %llu (must be 0)", (unsigned long long)r.rebuilds);
    report("static patch, new options", r.rebuilds == 0, buf);
  }
  struct V { const char* name; Cfg c; };
  std::vector<V> vs;
  { Cfg c; c.M = 3; c.seconds = 0.5; c.hash = true; vs.push_back({"first-run design", c}); }
  { Cfg c; c.M = 3; c.seconds = 0.5; c.hash = true; c.read = 6; c.norm = 1; c.dc = 1; vs.push_back({"6-point, WT-16, DC", c}); }
  { Cfg c; c.M = 3; c.seconds = 0.5; c.hash = true; c.read = 8; c.spacing = 2; c.xf = 1; vs.push_back({"8-point, half-octave, fused", c}); }
  { Cfg c; c.M = 3; c.seconds = 0.5; c.hash = true; c.read = 6; c.xf = 2; vs.push_back({"6-point, scheme (iii)", c}); }
  { Cfg c; c.M = 3; c.seconds = 0.5; c.hash = true; c.read = 6; c.law = LINEAR; c.pv = 1; c.dc = 1; vs.push_back({"6-point, per-voice frame", c}); }
  for (auto& v : vs) {
    Result a = runCell(v.c), b = runCell(v.c);
    Cfg d = v.c; d.seed ^= 0xBEEF;
    Result e = runCell(d);
    char buf[200], name[80];
    std::snprintf(name, sizeof name, "determinism: %s", v.name);
    std::snprintf(buf, sizeof buf, "hash run1 %08x run2 %08x; control (other seed) %08x (must differ)", a.hash, b.hash, e.hash);
    report(name, a.hash == b.hash && a.hash != e.hash, buf);
  }
}

void runChecks() {
  std::printf("SELF-CHECKS\n");
  checkIfft();
  checkReads();
  checkCorners(64);
  checkCorners(1024);
  checkLinearEquivalence(64, 2, 1);
  checkLinearEquivalence(1024, 2, 1);
  checkLinearEquivalence(1024, 6, 2);
  checkPerVoiceBlend();
  checkNorm();
  checkBandLimit();
  checkXfadeEnd();
  checkStaticAndDeterminism();
  std::printf("SELF-CHECKS: %s\n", g_fail ? "FAIL" : "ALL PASS");
}

/* ------------------------------------------------------------------ driver ------------------- */
struct Measured { double instr = 0, rebuilds = 0, window = 0; uint32_t hash = 0; bool ok = false; };

std::string cfgArgs(const Cfg& c, double seconds) {
  char b[400];
  std::snprintf(b, sizeof b, "--H %d --law %d --M %d --moving %d --fs %d --seconds %g --ablate %u --maxn %d --read %d --spacing %d --norm %d --dc %d --pv %d --xfade %d --xstep %d",
                c.H, c.law, c.M, c.moving ? 1 : 0, c.fs, seconds, c.ablate, c.maxN, c.read, c.spacing, c.norm, c.dc, c.pv, c.xf, c.xstep);
  return b;
}

/* One child process: /usr/bin/time -l <self> --cell ...; returns instructions retired + CELL line. */
Measured runChild(const std::string& exe, const Cfg& c, double seconds) {
  std::string cmd = "nice -n 10 /usr/bin/time -l '" + exe + "' --cell " + cfgArgs(c, seconds) + " 2>&1";
  Measured m;
  FILE* p = popen(cmd.c_str(), "r");
  if (!p) return m;
  char line[512];
  bool gotI = false, gotC = false;
  while (std::fgets(line, sizeof line, p)) {
    unsigned long long iv, rb;
    double win;
    unsigned h;
    if (std::strstr(line, "instructions retired") && std::sscanf(line, " %llu", &iv) == 1) { m.instr = double(iv); gotI = true; }
    if (std::sscanf(line, "CELL rebuilds=%llu hash=%x window=%lf", &rb, &h, &win) == 3) { m.rebuilds = double(rb); m.window = win; m.hash = h; gotC = true; }
  }
  m.ok = pclose(p) == 0 && gotI && gotC;
  return m;
}

/* Window instructions = (with window) - (no-render baseline: start-up, tables, warm-up). */
Measured measureCell(const std::string& exe, const Cfg& c, double seconds) {
  Measured base = runChild(exe, c, 0.0), full = runChild(exe, c, seconds);
  Measured r = full;
  r.instr = full.instr - base.instr;
  r.ok = base.ok && full.ok;
  return r;
}

const char* lawName(int l) { return l ? "Spectral log" : "Linear"; }

std::string describe(const Cfg& c) {
  char b[160];
  std::snprintf(b, sizeof b, "%s%s read%d/%s%s%s%s xf%d", lawName(c.law), c.pv ? "+pv-frame" : "", c.read, c.spacing == 2 ? "half-oct" : "oct",
                c.norm ? " WT16" : " fix.85", c.dc ? " dc" : "", c.maxN ? " cap*" : "", c.xf);
  return b;
}

/* The follow-up cells (the design WT-5/6/6a/15/16/23/25 specify). H=1024, M=9, moving, 48 kHz. */
std::vector<Cfg> followupCells() {
  std::vector<Cfg> v;
  Cfg prev;   // the first run's worst cell, for comparison
  v.push_back(prev);
  for (int sp : {1, 2}) {
    for (int mode = 0; mode < 3; ++mode) {   // Spectral log, Linear, Linear + per-voice frame
      Cfg c;
      c.read = 6; c.norm = 1; c.dc = 1; c.spacing = sp;
      c.law = mode == 0 ? SPECLOG : LINEAR;
      c.pv = mode == 2;
      v.push_back(c);
    }
  }
  for (int xf : {1, 2}) { Cfg c; c.read = 6; c.norm = 1; c.dc = 1; c.xf = xf; v.push_back(c); }   // Spectral log, octave
  { Cfg c; c.read = 8; c.norm = 1; c.dc = 1; v.push_back(c); }                                     // the 8-point / 4096 fallback
  { Cfg c; c.read = 6; c.norm = 1; c.dc = 1; c.moving = false; v.push_back(c); }                    // static, for reads alone
  return v;
}

int rowOf(const std::string& exe, const Cfg& c, double ips, int& bad) {
  Measured m = measureCell(exe, c, 2.0);
  const std::string d = describe(c);
  if (!m.ok) { std::printf("%-5d %-52s %2d %-7s %5d  CHILD FAILED\n", c.H, d.c_str(), c.M, c.moving ? "moving" : "static", c.fs); ++bad; return 1; }
  if (!c.moving && m.rebuilds != 0) { std::printf("  FAIL static cell rebuilt %.0f times in the window\n", m.rebuilds); ++bad; }
  double pct = m.instr / m.window / ips * 100;
  std::printf("%-5d %-52s %2d %-7s %5d %8.4f %14.0f %9.1f %7.2f %7.2f\n", c.H, d.c_str(), c.M, c.moving ? "moving" : "static", c.fs, m.window, m.instr, m.rebuilds / m.window, pct, pct * 2);
  std::fflush(stdout);
  return 0;
}

void tableHeader(double ips) {
  std::printf("(instructions retired in the measured window = with-window child minus no-render child; CPU%% = instr / window / ips x 100,\n"
              " ESTIMATE at ips %.1fe9, and at half that; PF-3 chord, 512-sample blocks; 'cap*' = --maxn given)\n", ips / 1e9);
  std::printf("%-5s %-52s %2s %-7s %5s %8s %14s %9s %7s %7s\n", "H", "design", "M", "motion", "fs", "window s", "instr", "rebuild/s", "CPU%", "CPU%/2");
}

int breakdown(const std::string& exe, const Cfg& b) {
  std::printf("\nCOST BREAKDOWN of %s (H=%d M=%d, %s): instructions removed by switching one stage off\n(stages interact through the rebuild/crossfade state; read as shares, not an exact partition)\n",
              describe(b).c_str(), b.H, b.M, b.moving ? "moving" : "static");
  Measured all = measureCell(exe, b, 2.0);
  struct Ab { const char* name; unsigned bit; };
  for (Ab a : {Ab{"coupling (Kuramoto tick)", AB_COUPLING}, Ab{"voice/member reads", AB_READ}, Ab{"blend law (per-harmonic)", AB_BLEND}, Ab{"inverse FFTs", AB_IFFT}}) {
    Cfg x = b; x.ablate = a.bit;
    Measured m = measureCell(exe, x, 2.0);
    std::printf("  %-28s %14.0f instr  (%5.1f %% of the cell)\n", a.name, all.instr - m.instr, 100 * (all.instr - m.instr) / all.instr);
  }
  std::printf("  %-28s %14.0f instr\n", "whole cell", all.instr);
  return 0;
}

int tableMain(const std::string& exe, int maxN, double ips) {
  std::printf("\nCELL TABLE, the first run's matrix (default design: 2-point read, one level per octave, fixed 0.85)\n");
  tableHeader(ips);
  int bad = 0;
  for (int H : {64, 1024})
    for (int law : {LINEAR, SPECLOG})
      for (int M : {1, 3, 9})
        for (bool moving : {false, true}) {
          Cfg c; c.H = H; c.law = law; c.M = M; c.moving = moving; c.maxN = maxN;
          rowOf(exe, c, ips, bad);
        }
  Cfg w; w.fs = 44100; w.maxN = maxN;
  rowOf(exe, w, ips, bad);
  w.moving = false;
  rowOf(exe, w, ips, bad);
  Cfg b; b.maxN = maxN;
  breakdown(exe, b);
  return bad ? 1 : 0;
}

int followupMain(const std::string& exe, double ips) {
  std::printf("\nFOLLOW-UP CELLS (H=1024, M=9, 48 kHz; the design WT-5/6/6a/15/16/23/25 specify)\n");
  tableHeader(ips);
  int bad = 0;
  for (const Cfg& c : followupCells()) rowOf(exe, c, ips, bad);
  Cfg w; w.read = 6; w.norm = 1; w.dc = 1; w.fs = 44100;
  rowOf(exe, w, ips, bad);
  Cfg s; s.read = 6; s.norm = 1; s.dc = 1;
  breakdown(exe, s);
  Cfg l; l.read = 6; l.norm = 1; l.dc = 1; l.law = LINEAR; l.pv = 1;
  breakdown(exe, l);
  return bad ? 1 : 0;
}

/* UNRUN by design (ADR-380 A1: timed runs only in the 2-6 am quiet window). Thread CPU time, best
   of 5, in-process. `--timed` alone: the first run's matrix. `--timed --followup`: the follow-up
   cells. `--timed` plus any cell option (--H --law --M --moving --fs --read --spacing --norm --dc
   --pv --xfade --xstep --maxn): that one cell. */
double timedCell(const Cfg& c) {
  auto tcpu = [] { timespec ts; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; };
  double best = 1e30, win = 0;
  for (int rep = 0; rep < 5; ++rep) {
    World w(c);
    Sim sim(c, w.fft, w.tp, w.g);
    const int warm = int(std::ceil(0.25 * c.fs / BLOCK)), meas = int(std::ceil(2.0 * c.fs / BLOCK));
    for (int b = 0; b < warm; ++b) sim.block();
    double t0 = tcpu();
    for (int b = 0; b < meas; ++b) sim.block();
    best = std::min(best, tcpu() - t0);
    win = double(meas) * BLOCK / c.fs;
  }
  return best / win * 100;
}

int timedMain(const Cfg& one, bool oneGiven, bool followup, int maxN) {
  std::printf("TIMED (thread CPU time, best of 5; hardware as run). QUIET WINDOW ONLY.\n");
  std::printf("%-5s %-52s %2s %-7s %5s %s\n", "H", "design", "M", "motion", "fs", "% of one core");
  auto row = [&](const Cfg& c) {
    std::printf("%-5d %-52s %2d %-7s %5d %8.3f\n", c.H, describe(c).c_str(), c.M, c.moving ? "moving" : "static", c.fs, timedCell(c));
    std::fflush(stdout);
  };
  if (oneGiven) row(one);
  else if (followup) { for (const Cfg& c : followupCells()) row(c); Cfg w; w.read = 6; w.norm = 1; w.dc = 1; w.fs = 44100; row(w); }
  else
    for (int H : {64, 1024})
      for (int law : {LINEAR, SPECLOG})
        for (int M : {1, 3, 9})
          for (bool moving : {false, true}) { Cfg c; c.H = H; c.law = law; c.M = M; c.moving = moving; c.maxN = maxN; row(c); }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  Cfg c;
  bool cell = false, checks = false, timed = false, followup = false, oneGiven = false;
  double ips = 10e9;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() { return i + 1 < argc ? argv[++i] : (char*)"0"; };
    auto opt = [&](int& dst) { dst = std::atoi(next()); oneGiven = true; };
    if (a == "--cell") cell = true;
    else if (a == "--checks") checks = true;
    else if (a == "--table") {}
    else if (a == "--followup") followup = true;
    else if (a == "--timed") timed = true;
    else if (a == "--H") opt(c.H);
    else if (a == "--law") opt(c.law);
    else if (a == "--M") opt(c.M);
    else if (a == "--moving") { c.moving = std::atoi(next()) != 0; oneGiven = true; }
    else if (a == "--fs") opt(c.fs);
    else if (a == "--seconds") c.seconds = std::atof(next());
    else if (a == "--ablate") c.ablate = unsigned(std::atoi(next()));
    else if (a == "--maxn") opt(c.maxN);
    else if (a == "--read") opt(c.read);
    else if (a == "--spacing") opt(c.spacing);
    else if (a == "--norm") opt(c.norm);
    else if (a == "--dc") opt(c.dc);
    else if (a == "--pv") opt(c.pv);
    else if (a == "--xfade") opt(c.xf);
    else if (a == "--xstep") opt(c.xstep);
    else if (a == "--ips") ips = std::atof(next());
    else if (a == "--hash") c.hash = true;
    else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
  }
  if (c.read != 2 && c.read != 6 && c.read != 8) { std::fprintf(stderr, "--read must be 2, 6 or 8\n"); return 2; }
  if (c.spacing != 1 && c.spacing != 2) { std::fprintf(stderr, "--spacing must be 1 or 2\n"); return 2; }
  if (c.pv && c.law != LINEAR) { std::fprintf(stderr, "--pv is Linear-only (WT-23)\n"); return 2; }
  if (cell) {
    Result r = runCell(c);
    std::printf("CELL rebuilds=%llu hash=%08x window=%.6f peak=%llu cap=%llu\n", (unsigned long long)r.rebuilds, r.hash, r.windowSec,
                (unsigned long long)r.peakEng, (unsigned long long)r.capEng);
    return 0;
  }
  if (timed) return timedMain(c, oneGiven && !followup, followup, c.maxN);
  if (checks) { runChecks(); return g_fail ? 1 : 0; }
  runChecks();
  if (g_fail) return 1;
  char* rp = realpath(argv[0], nullptr);
  std::string exe = rp ? rp : argv[0];
  std::free(rp);
  return followup ? followupMain(exe, ips) : tableMain(exe, c.maxN, ips);
}
