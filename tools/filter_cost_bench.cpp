/*
 * filter_cost_bench.cpp — what each filter type costs per voice, in Release C++ (B303, B566).
 * HYPERSAW / horde, 2026-10-28. Origin: ROADMAP B566 criterion (3): "every type's cost per voice
 * measured in a Release C++ bench (tools/, transcriptions of the lab JS, stated as such),
 * 16 voices × 2 filters at 48 kHz, as % of one core, and a proposed per-note type list".
 *
 * NOT A GATE and NOT THE PORT. Every class below is a TRANSCRIPTION of the per-voice types in
 * docs/design/filter-lab.html's DSP section (Svf, Svf12, Svf24, Ladder in its 2x Os with the
 * HalfBand pair, Ms20 with centred first-order ADAA, Comb with the Lagrange read, Formant, Ap4,
 * Phaser, Dj), line for line, in double precision as the lab runs them. It exists so the cost
 * B303 asks for is a Release number instead of a browser timing (the lab's own COST column is
 * relative and timed in JS). The swarm bank types (SWARM REF / PROPOSED) are rack-only, never
 * per voice, and need the protected reference's physics, so they are not here.
 *
 * TRANSCRIPTION IS CHECKED, NOT ASSUMED. `--dump` renders every type through one seeded stimulus
 * (mulberry32 noise, cutoff swept, set() every 16 samples) and prints the samples;
 * tools/labharness/filter_cost_parity.mjs slices the lab's DSP, renders the same stimulus and
 * compares. A cost measured on code that does not compute what the lab computes would be a cost
 * of something else.
 *
 * WHAT IS TIMED. 16 voices × 2 instances of one type (32 instances), each fed its voice's input,
 * set() once per 16-sample control tick with a moving cutoff (so coefficient work is in the
 * number, as in the lab, where every voice's filters are set every tick by its envelope and
 * keytrack), tick() per sample, outputs summed into a sink. CPU time is THREAD CPU time
 * (CLOCK_THREAD_CPUTIME_ID), not wall time, because this machine runs other agents' jobs: wall
 * time would charge us for their time slices. % of one core = cpu seconds / audio seconds × 100.
 * Median of `reps` runs after one warm-up. A BYPASS row (the same loop, no filter) is the
 * harness's own overhead; the per-type rows are reported raw, with the bypass beside them.
 *
 * ONE COMMAND, from the repo root (standalone; never through the plugin's CMake):
 *   clang++ -std=c++20 -O3 -o "$TMPDIR/fcb" tools/filter_cost_bench.cpp && nice -n 10 "$TMPDIR/fcb" 4 7
 * (args: seconds of audio, repeats). Parity against the lab, after building:
 *   "$TMPDIR/fcb" --dump | node tools/labharness/filter_fidelity_check.mjs --parity
 * The figures quoted in the lab (2026-10-28) were MEASURED UNDER HEAVY EXTERNAL LOAD (load avg
 * ~180), INDICATIVE ONLY; re-run on an unloaded machine before any of them is relied on.
 * A MEASUREMENT, NOT A CHECK: it asserts nothing and is not run by ./verify (it needs a C++
 * compile and its numbers are machine-dependent; B566 brief: "It need not be a gate").
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace {

constexpr double PI = 3.141592653589793;
constexpr double TAU = 6.283185307179586;
constexpr double SQRT2 = 1.4142135623730951;   // Math.SQRT2
constexpr double LN2 = 0.6931471805599453;     // Math.LN2
constexpr int TICK = 16;

/* mulberry32, bit-exact with the lab's (uint32 arithmetic replaces Math.imul and >>>). */
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

/* P, the lab's six knobs. */
struct P {
  double cutoff = 1000, oct = 0, res = 0.5, drive = 0, morph = 0.5;
  int mode = 1;
};

inline double clamp01(double x) { return std::max(0.0, std::min(1.0, x)); }
inline double clampFc(double sr, double f) { return std::max(10.0, std::min(0.49 * sr, f)); }
inline double fcOf(const P& p) { return p.cutoff * std::pow(2.0, p.oct); }
inline double driveGain(double d) { const double c = clamp01(d); return 1 + 15 * c * c; }

/* ---- Svf: the lab's transcription of src/svf_core.h, plus its NOTCH / PEAK taps and setK ---- */
struct Svf {
  double ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = SQRT2, fc = 1000;
  void set(double sr, double fcHz, double res) {
    const double f = std::max(10.0, std::min(0.49 * sr, fcHz));
    const double r = std::max(0.0, std::min(1.0, res));
    const double g = std::tan(PI * f / sr);
    const double kk = std::max(0.05, SQRT2 * (1 - r));
    fc = f; k = kk;
    a1 = 1 / (1 + g * (g + kk)); a2 = g * a1; a3 = g * a2;
  }
  void setK(double sr, double fcHz, double kk) {
    const double f = std::max(10.0, std::min(0.49 * sr, fcHz)), g = std::tan(PI * f / sr);
    fc = f; k = kk;
    a1 = 1 / (1 + g * (g + kk)); a2 = g * a1; a3 = g * a2;
  }
  double process(double x, int mode) {
    const double v3 = x - ic2;
    const double v1 = a1 * ic1 + a2 * v3;
    const double v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
    return mode == 1 ? v2 : mode == 2 ? v1 : mode == 3 ? x - k * v1 - v2
         : mode == 4 ? x - k * v1 : 2 * v2 - x + k * v1;
  }
};
inline double svfComp(double k, double k0, int mode) { return (mode == 2 || mode == 3 || mode == 5) ? std::sqrt(k / k0) : 1; }
const double K24A = 2 * std::cos(PI / 8), K24B = 2 * std::cos(3 * PI / 8);
inline double k24b(double res) { return std::max(0.05, K24B * (1 - clamp01(res))); }

struct Svf12 {
  Svf f; int m = 1; double g = 1;
  void set(double sr, const P& p) { f.set(sr, fcOf(p), p.res); m = p.mode ? p.mode : 1; g = svfComp(f.k, SQRT2, m); }
  double tick(double x) { return g * f.process(x, m); }
};
struct Svf24 {
  Svf s1, s2; int m = 1; double g = 1;
  void set(double sr, const P& p) { const double fc = fcOf(p), kb = k24b(p.res); s1.setK(sr, fc, K24A); s2.setK(sr, fc, kb); m = p.mode ? p.mode : 1; g = svfComp(kb, K24B, m); }
  double tick(double x) { return g * s2.process(s1.process(x, m), m); }
};

/* ---- ADAA (the centred first-order option on tanh's residual, 'adaa1c') ---- */
constexpr double AA_EPS = 1e-5;
inline double lcosh(double x) { const double a = std::fabs(x); return a + std::log1p(std::exp(-2 * a)) - LN2; }
inline double aaRes(double x) { return std::tanh(x) - x; }
inline double aaR(double x) {
  if (std::fabs(x) >= 0.1) return lcosh(x) - 0.5 * x * x;
  const double q = x * x;
  return q * q * (-1.0 / 12 + q * (1.0 / 45 + q * (-17.0 / 2520 + q * (31.0 / 14175 + q * (-691.0 / 935550 + q * 21844.0 / 85135050)))));
}
inline double aaD1R(double a, double b) { const double d = a - b; return std::fabs(d) > AA_EPS ? (aaR(a) - aaR(b)) / d : aaRes(0.5 * (a + b)); }
enum class Nl { none, adaa1c };
inline double nlEval(Nl nl, double a, double a1) {
  if (nl == Nl::none) return std::tanh(a);
  return a + aaD1R(a + 0.5 * (a - a1), 0.5 * (a + a1));
}

/* ---- LADDER (the lab's one-tanh ZDF ladder) ---- */
const double LADDER_KON = 4.2, LADDER_RON = 4 / 4.2, LADDER_KTOP = 4 * std::pow(1.05, 4);
inline double ladderK(double res) { const double r = clamp01(res); return r <= LADDER_RON ? LADDER_KON * r : 4 + (LADDER_KTOP - 4) * (r - LADDER_RON) / (1 - LADDER_RON); }
struct Ladder {
  double s[4] = {0, 0, 0, 0}, G = 0.5, k = 0, dg = 1, out = 1, a1 = 0, a2 = 0; Nl nl;
  explicit Ladder(Nl n = Nl::none) : nl(n) {}
  void set(double sr, const P& p) {
    const double g = std::tan(PI * clampFc(sr, fcOf(p)) / sr);
    G = g / (1 + g); k = ladderK(p.res); dg = driveGain(p.drive); out = 1 + k / 2;
  }
  double tick(double x) {
    const double S = (1 - G) * (((G * s[0] + s[1]) * G + s[2]) * G + s[3]);
    const double G4 = G * G * G * G;
    const double y4 = (G4 * x + S) / (1 + k * G4), a = dg * (x - k * y4);
    double u = nlEval(nl, a, a1) / dg; a2 = a1; a1 = a;
    for (int i = 0; i < 4; i++) { const double v = (u - s[i]) * G, y = v + s[i]; s[i] = y + v; u = y; }
    return u * out;
  }
};

/* ---- the 2x stage: 55-tap Kaiser (beta 10) half-band pair, polyphase ---- */
constexpr int HB_L = 55, HB_C = (HB_L - 1) / 2, HB_K = (HB_C + 1) / 2;
constexpr double HB_BETA = 10;
struct HbTable {
  double h[HB_K];
  HbTable() {
    auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 50; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
    for (int k = 0; k < HB_K; k++) { const int n = 2 * k + 1; h[k] = std::sin(PI * n / 2) / (PI * n) * i0(HB_BETA * std::sqrt(1 - (double(n) / HB_C) * (double(n) / HB_C))) / i0(HB_BETA); }
  }
};
const HbTable HB;
struct HalfBand {
  double x[32] = {}, e[32] = {}, o[32] = {}; int i = 0, j = 0;
  void up(double in, double out[2]) {
    const int ii = (i + 1) & 31; i = ii; x[ii] = in;
    double s = 0; for (int k = 0; k < HB_K; k++) s += HB.h[k] * (x[(ii - HB_K - k) & 31] + x[(ii - HB_K + 1 + k) & 31]);
    out[0] = 2 * s; out[1] = x[(ii - HB_K + 1) & 31];
  }
  double down(double ev, double od) {
    const int jj = (j + 1) & 31; j = jj; e[jj] = ev; o[jj] = od;
    double s = 0.5 * o[(jj - HB_K) & 31]; for (int k = 0; k < HB_K; k++) s += HB.h[k] * (e[(jj - HB_K - k) & 31] + e[(jj - HB_K + 1 + k) & 31]);
    return s;
  }
};
template <class Inner> struct Os2 {
  Inner inner; HalfBand hb;
  explicit Os2(Inner in) : inner(in) {}
  void set(double sr, const P& p) { inner.set(sr * 2, p); }
  double tick(double x) { double t[2]; hb.up(x, t); const double ya = inner.tick(t[0]); return hb.down(ya, inner.tick(t[1])); }
};

/* ---- MS-20 / Sallen-Key ---- */
constexpr double SK_KMAX = 2.1;
struct Ms20 {
  double s1 = 0, s2 = 0, s3 = 0, G = 0.5, K = 0, dg = 1, a1 = 0, a2 = 0; Nl nl;
  explicit Ms20(Nl n = Nl::none) : nl(n) {}
  void set(double sr, const P& p) { const double g = std::tan(PI * clampFc(sr, fcOf(p)) / sr); G = g / (1 + g); K = SK_KMAX * clamp01(p.res); dg = driveGain(p.drive); }
  double tick(double x) {
    const double H = 1 - G;
    const double y1 = G * x + H * s1;
    const double yl = (G * y1 - G * K * H * s3 + H * s2) / (1 - G * K * H), a = dg * K * H * (yl - s3);
    const double u = y1 + nlEval(nl, a, a1) / dg; a2 = a1; a1 = a;
    const double v1 = (x - s1) * G; s1 = y1 + v1;
    const double v2 = (u - s2) * G, y = v2 + s2; s2 = y + v2;
    const double v3 = (y - s3) * G; s3 = v3 + s3 + v3;
    return y;
  }
};

/* ---- COMB (Lagrange read) ---- */
constexpr int COMB_N = 8192; constexpr double COMB_GMAX = 0.98;
inline void lagr3(double fr, double h[4]) { const double d = 1 + fr; h[0] = -(d - 1) * (d - 2) * (d - 3) / 6; h[1] = d * (d - 2) * (d - 3) / 2; h[2] = -d * (d - 1) * (d - 3) / 2; h[3] = d * (d - 1) * (d - 2) / 6; }
struct Comb {
  int sign; std::vector<double> buf; int w = 0, N = 48; double h[4] = {}, g = 0, norm = 1;
  explicit Comb(int s) : sign(s), buf(COMB_N, 0.0) { lagr3(0, h); }
  void set(double sr, const P& p) {
    const double D = std::max(2.0, std::min(double(COMB_N - 2), sr / std::max(1.0, fcOf(p))));
    N = int(std::floor(D)); lagr3(D - N, h);
    g = sign * COMB_GMAX * clamp01(p.res); norm = 1 - std::fabs(g);
  }
  double tick(double x) {
    const int m = COMB_N - 1, i = w - N + 1; const double* b = buf.data();
    const double d = h[0] * b[i & m] + h[1] * b[(i - 1) & m] + h[2] * b[(i - 2) & m] + h[3] * b[(i - 3) & m];
    const double y = x + g * d; buf[w] = y; w = (w + 1) & m;
    return y * norm;
  }
};

/* ---- FORMANT ---- */
const double VOWELS[5][3][3] = {
  {{600, 0, 60}, {1040, -7, 70}, {2250, -9, 110}},
  {{400, 0, 40}, {1620, -12, 80}, {2400, -9, 100}},
  {{250, 0, 60}, {1750, -30, 90}, {2600, -16, 100}},
  {{400, 0, 40}, {750, -11, 80}, {2400, -21, 100}},
  {{350, 0, 40}, {600, -20, 80}, {2400, -32, 100}},
};
struct Formant {
  Svf f[3]; double fc[3] = {}, k[3] = {}, a[3] = {};
  void set(double sr, const P& p) {
    const double pp = clamp01(p.morph) * 4; const int i0 = std::min(3, int(std::floor(pp))); const double t = pp - i0;
    const double shift = fcOf(p) / 1000, qs = std::pow(2.0, 2 * (clamp01(p.res) - 0.5)), qg = std::sqrt(qs);
    for (int j = 0; j < 3; j++) {
      const double* A = VOWELS[i0][j]; const double* B = VOWELS[i0 + 1][j];
      const double F = A[0] + (B[0] - A[0]) * t, dB = A[1] + (B[1] - A[1]) * t, BW = A[2] + (B[2] - A[2]) * t;
      fc[j] = clampFc(sr, F * shift); k[j] = BW / F / qs; a[j] = std::pow(10.0, dB / 20) * qg;
    }
    for (int j = 0; j < 3; j++) f[j].setK(sr, fc[j], k[j]);
  }
  double tick(double x) { double y = 0; for (int j = 0; j < 3; j++) y += a[j] * k[j] * f[j].process(x, 2); return y; }
};

/* ---- ALL-PASS x4 (the normalised lattice) and PHASER ---- */
struct Ap4 {
  double s[4] = {0, 0, 0, 0}, a = 0, c = 1;
  void set(double sr, const P& p) { const double g = std::tan(PI * clampFc(sr, fcOf(p)) / sr); a = (1 - g) / (1 + g); c = std::sqrt(std::max(0.0, 1 - a * a)); }
  double tick(double x) { for (int i = 0; i < 4; i++) { const double y = c * s[i] - a * x; s[i] = c * x + a * s[i]; x = y; } return x; }
};
constexpr double PHASER_FBMAX = 0.9;
struct Phaser {
  Ap4 ap; double fb = 0, w = 0;
  void set(double sr, const P& p) { ap.set(sr, p); fb = PHASER_FBMAX * clamp01(p.res); }
  double tick(double x) { const double y = ap.tick(x + fb * w); w = y; return 0.5 * (x + y); }
};

/* ---- DJ ---- */
constexpr double DJ_LO = 50, DJ_HI = 12000, DJ_XF = 0.1;
struct Dj {
  Svf f; int side = 1; double w = 0;
  void set(double sr, const P& p) {
    const double m = clamp01(p.morph); const bool lp = m < 0.5; const double t = lp ? (0.5 - m) / 0.5 : (m - 0.5) / 0.5;
    const double fc = (lp ? 20000 * std::pow(DJ_LO / 20000, t) : 20 * std::pow(DJ_HI / 20, t)) * std::pow(2.0, p.oct);
    side = lp ? 1 : 3; w = std::min(1.0, t / DJ_XF); f.set(sr, fc, p.res);
  }
  double tick(double x) { const double y = f.process(x, side); return x + w * (y - x); }
};

/* ---- the registry, in the lab's TYPE_ORDER (rack-only bank types excluded) ---- */
struct Row { const char* id; int kind; int mode; };
// kind: 0 svf12, 1 svf24, 2 ladder (os2, as TYPES.ladder.aa), 3 ms20 (adaa1c, as TYPES.ms20.aa), 4 combP, 5 combN, 6 formant, 7 ap4, 8 phaser, 9 dj,
//       10 ladder at 1x (no anti-aliasing: what the oversampling costs, reported only)
const Row ROWS[] = {
  {"svf12.LP", 0, 1}, {"svf12.BP", 0, 2}, {"svf12.HP", 0, 3}, {"svf12.NOTCH", 0, 4}, {"svf12.PEAK", 0, 5},
  {"svf24.LP", 1, 1}, {"svf24.BP", 1, 2}, {"svf24.HP", 1, 3}, {"svf24.NOTCH", 1, 4}, {"svf24.PEAK", 1, 5},
  {"ladder", 2, 1}, {"ms20", 3, 1}, {"combP", 4, 1}, {"combN", 5, 1}, {"formant", 6, 1},
  {"ap4", 7, 1}, {"phaser", 8, 1}, {"dj", 9, 1},
};
const Row LADDER_1X = {"ladder@1x (no AA, reported only)", 10, 1};

/* The stimulus every row sees, bit-identical to the parity script's: P per tick (cutoff 1 kHz
   swept ±2 octaves at 3 Hz, res 0.7, drive 0.3, morph 0.3), x = (rn()·2 − 1)·0.5. */
inline void stimP(P& p, int n, int mode, double sr) {
  p.cutoff = 1000; p.oct = 2 * std::sin(TAU * 3 * n / sr); p.res = 0.7; p.drive = 0.3; p.morph = 0.3; p.mode = mode;
}

template <class F> void dumpOne(const char* id, F f, int mode) {
  const double sr = 48000; Mulberry32 rn(0xB566u); P p;
  std::printf("%s", id);
  for (int n = 0; n < 4096; n++) {
    if (n % TICK == 0) { stimP(p, n, mode, sr); f.set(sr, p); }
    const double x = (rn() * 2 - 1) * 0.5;
    std::printf(" %.17g", f.tick(x));
  }
  std::printf("\n");
}

double cpuNow() { timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return t.tv_sec + 1e-9 * t.tv_nsec; }

/* 16 voices × 2 instances, `secs` of audio. Each voice's cutoff follows its own note and a
   per-voice envelope-like sweep, recomputed every tick, as the lab's voiceFilters does. */
template <class F, class Mk> double timeIt(Mk mk, int mode, double secs, int reps, double* sinkOut) {
  constexpr int V = 16, NF = 2, BUF = 4096; const double sr = 48000;
  std::vector<double> noise(BUF); { Mulberry32 rn(0xC057u); for (auto& v : noise) v = (rn() * 2 - 1) * 0.5; }
  const int N = int(secs * sr) / TICK * TICK;
  std::vector<double> runs;
  double sink = 0;
  for (int r = 0; r <= reps; r++) {
    std::vector<F> inst; inst.reserve(V * NF);
    for (int i = 0; i < V * NF; i++) inst.push_back(mk());
    P p; p.res = 0.5; p.drive = 0.3; p.morph = 0.3; p.mode = mode;
    const double t0 = cpuNow();
    for (int n = 0; n < N; n += TICK) {
      for (int v = 0; v < V; v++) {
        const double note = 36 + 3 * v;                                  // a spread chord, C2 up
        const double env = 0.5 + 0.5 * std::sin(TAU * (0.7 + 0.11 * v) * n / sr);
        for (int f = 0; f < NF; f++) {
          p.cutoff = f ? 1800 : 700; p.oct = 0.5 * (note - 60) / 12 + 3 * env;
          inst[v * NF + f].set(sr, p);
        }
        const int base = (n + 97 * v) & (BUF - 1);
        double acc = 0;
        F& a = inst[v * NF]; F& b = inst[v * NF + 1];
        for (int i = 0; i < TICK; i++) { const double xi = noise[(base + i) & (BUF - 1)]; acc += a.tick(xi) + b.tick(xi); }
        sink += acc;
      }
    }
    const double t1 = cpuNow();
    if (r) runs.push_back(t1 - t0);
  }
  std::sort(runs.begin(), runs.end());
  *sinkOut += sink;
  return runs[runs.size() / 2] / (double(N) / sr) * 100;            // % of one core
}

struct Bypass { void set(double, const P&) {} double tick(double x) { return x; } };

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--dump") {
    for (const Row& r : ROWS) {
      switch (r.kind) {
        case 0: dumpOne(r.id, Svf12(), r.mode); break;
        case 1: dumpOne(r.id, Svf24(), r.mode); break;
        case 2: dumpOne(r.id, Os2<Ladder>(Ladder(Nl::none)), r.mode); break;
        case 3: dumpOne(r.id, Ms20(Nl::adaa1c), r.mode); break;
        case 4: dumpOne(r.id, Comb(1), r.mode); break;
        case 5: dumpOne(r.id, Comb(-1), r.mode); break;
        case 6: dumpOne(r.id, Formant(), r.mode); break;
        case 7: dumpOne(r.id, Ap4(), r.mode); break;
        case 8: dumpOne(r.id, Phaser(), r.mode); break;
        case 9: dumpOne(r.id, Dj(), r.mode); break;
      }
    }
    return 0;
  }
  const double secs = argc > 1 ? std::atof(argv[1]) : 4.0;
  const int reps = argc > 2 ? std::atoi(argv[2]) : 7;
  double sink = 0;
  std::printf("filter_cost_bench: 16 voices x 2 filters, 48 kHz, set() every %d samples, %.1f s of audio, median of %d (thread CPU time)\n", TICK, secs, reps);
  std::printf("%-34s %10s %12s\n", "type", "% of core", "ns/inst/smp");
  auto line = [&](const char* id, double pc) {
    const double ns = pc / 100 * 1e9 / (48000.0 * 32);
    std::printf("%-34s %10.3f %12.2f\n", id, pc, ns);
  };
  line("BYPASS (harness overhead)", timeIt<Bypass>([] { return Bypass(); }, 1, secs, reps, &sink));
  for (const Row& r : ROWS) {
    double pc = 0;
    switch (r.kind) {
      case 0: pc = timeIt<Svf12>([] { return Svf12(); }, r.mode, secs, reps, &sink); break;
      case 1: pc = timeIt<Svf24>([] { return Svf24(); }, r.mode, secs, reps, &sink); break;
      case 2: pc = timeIt<Os2<Ladder>>([] { return Os2<Ladder>(Ladder(Nl::none)); }, r.mode, secs, reps, &sink); break;
      case 3: pc = timeIt<Ms20>([] { return Ms20(Nl::adaa1c); }, r.mode, secs, reps, &sink); break;
      case 4: pc = timeIt<Comb>([] { return Comb(1); }, r.mode, secs, reps, &sink); break;
      case 5: pc = timeIt<Comb>([] { return Comb(-1); }, r.mode, secs, reps, &sink); break;
      case 6: pc = timeIt<Formant>([] { return Formant(); }, r.mode, secs, reps, &sink); break;
      case 7: pc = timeIt<Ap4>([] { return Ap4(); }, r.mode, secs, reps, &sink); break;
      case 8: pc = timeIt<Phaser>([] { return Phaser(); }, r.mode, secs, reps, &sink); break;
      case 9: pc = timeIt<Dj>([] { return Dj(); }, r.mode, secs, reps, &sink); break;
    }
    line(r.id, pc);
  }
  line(LADDER_1X.id, timeIt<Ladder>([] { return Ladder(Nl::none); }, 1, secs, reps, &sink));
  std::printf("(sink %.6g — printed so the optimiser keeps every tick)\n", sink);
  return 0;
}
