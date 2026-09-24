/*
 * measure_modsources — computational-cost bench for two CANDIDATE modulation
 * sources that exist only as JS: the Kuro LFO (ADR-053) and ORBITAL
 * (ADR-165, specs/SPEC-ORBITAL.md). ROADMAP B236 (human 2026-09-23: "let's
 * test the computational costs of orbital and kuramoto LFO, just to make sure
 * they wouldn't break anything").
 *
 * UNWIRED: this is a timing measurement, not a pass/fail gate (the
 * measure_cpu/measure_alias contract, tools/measure_cpu.cpp's own header) —
 * it prints, never judges, and carries no threshold to weaken. It is also
 * OUTSIDE tools/test_table_check.py's census: that gate only globs files
 * named "_check.cpp", "_check.py" (tools/) and "_check.mjs" (tools/labharness/);
 * this file is named measure_modsources.cpp, the same shape as
 * measure_cpu.cpp and measure_alias.cpp, both of which the census also does
 * not see (confirmed: `python3 tools/test_table_check.py` after this file
 * lands still reports the same wired count, and none of the three match the
 * glob).
 *
 * PORTS, not the shipped engine. Neither law is in src/ yet — both are
 * transcribed here from docs/design/shape-lab-mod.html (B226 round 2), which
 * is itself a faithful transcription of two other things:
 *   - Kuro LFO: docs/design/mod-lab.html's KuroSwarm (ADR-053), the subset
 *     shape-lab-mod.html keeps (n 1..8, bipolar K, rate/detune/anchor, the
 *     mean-field topology, the rank-lattice splay), with the splay's rank
 *     order done as an insertion sort into a preallocated array rather than
 *     mod-lab's per-sample allocate-and-sort — same permutation, cheaper.
 *     trace 2026-09-23-b226-modulator-lab-2.md measured this bit-identical to
 *     mod-lab's own class over 1,056 configs, so shape-lab-mod.html IS the law.
 *   - ORBITAL: reference/gravity-modulator.html's accel()/step()
 *     (:307-323, :397-449) with the edge cushion, both B126 regulators and
 *     pointer drag left out (all four are off in every reference preset), and
 *     the same trace measured that transcription bit-identical to the
 *     reference's own step() over four presets. specs/SPEC-ORBITAL.md §6.2
 *     is where the plugin's OWN divergence lives: dt = 1/960 s (not the
 *     reference/lab's 1/480), sample-count accumulation, so offline ==
 *     realtime for a given start state. This file benchmarks the PLUGIN's
 *     dt (1/960, §5/§11) — the finer, more expensive of the two — and keeps
 *     dt = 1/480 only for the parity dump below, so the parity claim is not
 *     confused with the shipped rate (ADR-165 amendment of 2026-09-17: the
 *     port's 1/960 is not a parity claim against the browser).
 *
 * PARITY (`--parity`): prints deterministic trajectories for both laws at a
 * fixed config so they can be diffed against docs/design/shape-lab-mod.html
 * run under Node (tools/golden/extract_core.mjs, 'design' banner set) on the
 * SAME inputs. This binary does not run Node itself — it has no such
 * dependency, and adding one to a benchmark would be exactly the kind of
 * invention the charter's "reduce, never invent" rule is for. The diff is a
 * one-time, by-hand verification recorded in the PR's trace, not a gate.
 *
 * THE CONTROL-TICK RATE. The brief's parenthetical asks to verify "16
 * samples" against src/: kTick=16 (swarm_core.h, filter_core.h, ...) is the
 * DSP-internal smoothing tick, not the rate a modulation source is read at.
 * The modulation/intent grid is kGravGridSeconds = 256/44100 s ≈ 172 Hz
 * (src/swarm_core.h:136, read at every hypersaw::kGravGridSeconds call site
 * in hypersaw_clap.cpp, and named explicitly as "the mod tick" in ROADMAP
 * B199: "the mod tick is kGravGridSeconds ≈ 172 Hz"). This file reports BOTH
 * — the literal 16-sample tick asked for, and the real ~172 Hz grid (its
 * sample count recomputed per rate via lround(sr * kGravGridSeconds), exactly
 * how the engine does it, ADR-009: seconds, never a per-tick constant) — and
 * flags the mismatch rather than silently picking one.
 *
 * CALIBRATION CONTROL. A fixed FLOP loop, `volatile`-sunk exactly as
 * tools/measure_cpu.cpp's refLoopMs(), timed alongside every run: if the
 * timer read zero for it, the optimiser ate the benchmark, not just this one.
 *
 * Release build only (global CLAUDE.md, "Test in Release, never Debug" — a
 * -O0 number here is exactly as meaningless as it is in measure_cpu.cpp).
 * Standalone: no CLAP link, no plugin factory — both laws are pure, sample-
 * driven state machines, so there is nothing here for the shell to add.
 */
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
double ms(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count() * 1000.0;
}

/* ---------------------------------------------------------------- calibration
 * Same shape as measure_cpu.cpp's refLoopMs(): a fixed, deterministic FLOP
 * loop whose cost is measured on THIS run, min of 3, `volatile`-sunk so the
 * optimiser cannot delete it. It is not compared to a recorded quiet-machine
 * figure (this file does not judge); it exists so a reader can see the timer
 * measuring real work before trusting the ns/step numbers below it. */
double calibLoopMs(long long iters) {
  double best = 1e9;
  for (int rep = 0; rep < 3; rep++) {
    volatile double sink = 0;
    const auto t0 = Clock::now();
    double acc = 0;
    for (long long i = 0; i < iters; i++) acc += std::sqrt((double)i + 1.0) * std::sin((double)i * 1e-6);
    sink = acc;
    (void)sink;
    const auto t1 = Clock::now();
    best = std::min(best, ms(t0, t1));
  }
  return best;
}

/* ================================================================
 * KURO LFO — docs/design/shape-lab-mod.html:754-843, KuroSwarm.
 * Arithmetic kept token for token (double throughout, as the lab already is);
 * only the host language changed. KURO_TAU is the lab's OWN truncated 2*pi
 * (itself inherited from mod-lab.html:78 on purpose, for bit-identity with
 * it) — kept here unchanged, since this is a port of the lab's law, not a
 * more-precise rewrite of it. Shape 4 (DRAWN) needs the LFO card's UI
 * breakpoints, which this benchmark never selects (shape stays 0, sine) —
 * left out rather than faked, per "reduce, never invent". */
constexpr double KURO_TAU = 6.283185307;
constexpr int KTICK = 16, MAXK = 8;

struct KuroSwarm {
  double sr;
  struct { int n; double K, gRate, detune; int anchor; } p{4, 0.35, 1.0, 0.3, 0};
  std::array<int, MAXK> shape{};
  std::array<double, MAXK> phase{}, lfo{}, rate{}, ratio{};
  std::array<int, MAXK> rank{};
  double R = 0, psi = 0;
  int tick = 0;

  explicit KuroSwarm(double sr_, int n) : sr(sr_) {
    p.n = n > 0 ? n : 4;
    initPhases();
    updateRates();
    controlTick();
  }
  void initPhases() {
    const int n = std::max(1, p.n);
    for (int i = 0; i < MAXK; i++) phase[i] = double(i % n) / n;
  }
  void updateRates() {
    const int n = std::max(1, p.n);
    double mn = 1e9, mx = -1e9, lg = 0.0;
    for (int i = 0; i < n; i++) {
      const double x = n == 1 ? 0.0 : (2.0 * i / (n - 1) - 1.0);
      const double r = std::pow(2.0, x * p.detune * 2.0);
      ratio[i] = r;
      lg += std::log(r);
      if (r < mn) mn = r;
      if (r > mx) mx = r;
    }
    const double norm = p.anchor == 1 ? mn : (p.anchor == 2 ? mx : std::exp(lg / n));
    for (int i = 0; i < n; i++) rate[i] = p.gRate * ratio[i] / norm;
    for (int i = n; i < MAXK; i++) rate[i] = 0.0;
  }
  double shapeOf(double ph, int vi) const {
    switch (shape[vi]) {
      case 1: return ph < 0.5 ? (4 * ph - 1) : (3 - 4 * ph);
      case 2: return 2 * ph - 1;
      case 3: return ph < 0.5 ? 1.0 : -1.0;
      default: return std::sin(KURO_TAU * ph);   // shape 0, and the stand-in for un-benchmarked shape 4
    }
  }
  void controlTick() {
    const int n = std::max(1, p.n);
    double cx = 0, cy = 0;
    for (int i = 0; i < n; i++) { const double a = KURO_TAU * phase[i]; cx += std::cos(a); cy += std::sin(a); }
    R = std::sqrt(cx * cx + cy * cy) / n;
    psi = std::atan2(cy, cx);
    for (int i = 0; i < n; i++) lfo[i] = shapeOf(phase[i], i);
    for (int i = n; i < MAXK; i++) lfo[i] = 0.0;
  }
  // Faithful port of advance(nSmp), including the K<0 rank-lattice/splay
  // branch's insertion sort — the comparator is JS's `(a || b) > 0`: use a
  // if it is nonzero, else b. Translated explicitly, not approximated.
  void advance(int nSmp) {
    const int n = std::max(1, p.n);
    const double K = p.K, srLocal = sr;
    const double kHz = (K > 0 ? K * K : -K * K) * 3.0;
    for (int smp = 0; smp < nSmp; smp++) {
      if (tick == 0) controlTick();
      tick = (tick + 1) & (KTICK - 1);
      if (K < 0) {
        const double kMag = -kHz, blend = -K;
        double mr = 0;
        for (int i = 0; i < n; i++) mr += rate[i];
        mr /= n;
        for (int j = 0; j < n; j++) rank[j] = j;
        for (int j = 1; j < n; j++) {
          const int x = rank[j];
          int q = j - 1;
          while (q >= 0) {
            const double diff = phase[rank[q]] - phase[x];
            const double cmp = diff != 0.0 ? diff : double(rank[q] - x);
            if (!(cmp > 0)) break;
            rank[q + 1] = rank[q];
            q--;
          }
          rank[q + 1] = x;
        }
        const double anchor = phase[rank[0]];
        for (int r = 0; r < n; r++) {
          const int i = rank[r];
          const double eff = rate[i] + (mr - rate[i]) * blend;
          const double adv = eff / srLocal + kMag * std::sin(KURO_TAU * (anchor + double(r) / n - phase[i])) / srLocal;
          phase[i] += adv;
        }
      } else {
        for (int i = 0; i < n; i++) {
          const double th = KURO_TAU * phase[i];
          double c = 0;
          if (K > 0) c = kHz * R * std::sin(psi - th);
          phase[i] += (rate[i] + c) / srLocal;
        }
      }
      for (int i = 0; i < n; i++) phase[i] -= std::floor(phase[i]);
    }
  }
};

/* ================================================================
 * ORBITAL — docs/design/shape-lab-mod.html:910-926 (accel), :927-957 (step).
 * Cushion, both B126 regulators, kick/reset/trail/smoothing are the lab's own
 * additions beyond the reference's step(); this benchmark only needs the
 * physics step the brief names, so those are left out (reduce, never
 * invent) — none of them changes the O(N^2) pair-loop cost this file exists
 * to measure. dtPhys and capSteps are runtime fields, not baked constants,
 * so the SAME code times both the lab's parity config (1/480 s, cap 8 — the
 * trace's own proven-bit-identical config) and the plugin's spec config
 * (1/960 s, §5/§11) without duplicating the class. */
constexpr int MAXB = 8;
constexpr double ORB_MAXV = 3.0;

struct OrbBody { double m, x, y, vx, vy, ax = 0, ay = 0; bool pinned = false; };

struct OrbitalField {
  std::vector<OrbBody> nodes;
  double G, eps, ts, damp;
  bool wrap, com;   // bounds: wrap=false => 'reflect' (the only other mode exercised here)
  double dtPhys;
  int capSteps;
  double acc = 0.0;
  long long droppedTicks = 0;   // counts a cap hit — must stay 0 in every config below (L0032)

  void accel() {
    const int n = (int)nodes.size();
    for (auto &b : nodes) { b.ax = 0; b.ay = 0; }
    const double e2 = eps * eps;
    for (int i = 0; i < n; i++) {
      auto &a = nodes[i];
      for (int j = i + 1; j < n; j++) {
        auto &b = nodes[j];
        double dx = b.x - a.x, dy = b.y - a.y;
        if (wrap) { if (dx > 0.5) dx -= 1; if (dx < -0.5) dx += 1; if (dy > 0.5) dy -= 1; if (dy < -0.5) dy += 1; }
        const double r2 = dx * dx + dy * dy + e2;
        const double inv = G / (r2 * std::sqrt(r2));
        a.ax += dx * inv * b.m; a.ay += dy * inv * b.m;
        b.ax -= dx * inv * a.m; b.ay -= dy * inv * a.m;
      }
    }
  }
  void step(double dt) {
    accel();
    for (auto &b : nodes) { if (b.pinned) continue; b.vx += 0.5 * b.ax * dt; b.vy += 0.5 * b.ay * dt; }
    for (auto &b : nodes) { if (b.pinned) continue; b.x += b.vx * dt; b.y += b.vy * dt; }
    accel();
    for (auto &b : nodes) {
      if (b.pinned) continue;
      b.vx += 0.5 * b.ax * dt; b.vy += 0.5 * b.ay * dt;
      if (damp > 0) { const double k = std::exp(-damp * 3 * dt); b.vx *= k; b.vy *= k; }
    }
    for (auto &b : nodes) {
      if (b.pinned) continue;
      if (b.vx * b.vx + b.vy * b.vy > ORB_MAXV * ORB_MAXV) {
        const double s = std::hypot(b.vx, b.vy);
        b.vx *= ORB_MAXV / s; b.vy *= ORB_MAXV / s;
      }
      if (!wrap) {
        if (b.x < 0) { b.x = -b.x; b.vx = std::abs(b.vx); } else if (b.x > 1) { b.x = 2 - b.x; b.vx = -std::abs(b.vx); }
        if (b.y < 0) { b.y = -b.y; b.vy = std::abs(b.vy); } else if (b.y > 1) { b.y = 2 - b.y; b.vy = -std::abs(b.vy); }
      } else {
        b.x = std::fmod(std::fmod(b.x, 1.0) + 1.0, 1.0);
        b.y = std::fmod(std::fmod(b.y, 1.0) + 1.0, 1.0);
      }
    }
    if (com && !wrap) {
      double M = 0, cx = 0, cy = 0, vx = 0, vy = 0;
      bool anyPinned = false;
      for (auto &b : nodes) { if (b.pinned) { anyPinned = true; continue; } M += b.m; cx += b.x * b.m; cy += b.y * b.m; vx += b.vx * b.m; vy += b.vy * b.m; }
      if (M > 0 && !anyPinned) {
        cx /= M; cy /= M; vx /= M; vy /= M;
        for (auto &b : nodes) { b.x += 0.5 - cx; b.y += 0.5 - cy; b.vx -= vx; b.vy -= vy; }
      }
    }
  }
  // One INVOCATION representing `nSamplesElapsed` audio samples at `sr` —
  // this is the knob that lets the same field be driven per-sample or per-
  // control-tick. Sample-count accumulation (SPEC-ORBITAL §6.2), never wall
  // time. Returns the physics-step count so a cap hit can be counted rather
  // than silently swallowed.
  int advance(double sr, int nSamplesElapsed) {
    acc += ts * nSamplesElapsed / sr;
    int k = 0;
    while (acc >= dtPhys && k < capSteps) { step(dtPhys); acc -= dtPhys; k++; }
    if (k >= capSteps) { acc = 0.0; droppedTicks++; }
    return k;
  }
};

// The benchmark's own 8-body config (worst case per SPEC-ORBITAL §3.1's
// "Body count 2-8" — none of the reference/lab's four presets uses 8, so
// this is a synthetic ring, not a preset; only the arithmetic is under test,
// not any particular preset's musicality). TAU here is the DSP section's own
// un-truncated 2*pi (shape-lab-mod.html:607), used for the ring's angles.
constexpr double TAU_FULL = 6.283185307179586;
std::vector<OrbBody> eightBodyRing() {
  std::vector<OrbBody> v;
  const double mass[MAXB] = {1.2, 0.8, 2.0, 0.5, 1.0, 0.6, 1.5, 0.9};
  for (int i = 0; i < MAXB; i++) {
    const double a = i * (TAU_FULL / MAXB);
    OrbBody b;
    b.m = mass[i];
    b.x = 0.5 + 0.3 * std::cos(a);
    b.y = 0.5 + 0.3 * std::sin(a);
    b.vx = -0.4 * std::sin(a);
    b.vy = 0.4 * std::cos(a);
    b.pinned = false;
    v.push_back(b);
  }
  return v;
}

/* ---------------------------------------------------------------- parity dump
 * Deterministic trajectories at a fixed config, for a by-hand diff against
 * docs/design/shape-lab-mod.html run under Node (extract_core.mjs, 'design'
 * banner). Not a gate — see the file header. */
void runParityDump() {
  std::printf("== KURO parity: n=8, detune=0.3, anchor=0, 4800 samples @ 44100 ==\n");
  for (double K : {-0.6, 0.6}) {
    KuroSwarm ks(44100.0, 8);
    ks.p.K = K;
    ks.advance(4800);
    std::printf("K=%+.1f phase:", K);
    for (int i = 0; i < 8; i++) std::printf(" %.17g", ks.phase[i]);
    std::printf("\nK=%+.1f   lfo:", K);
    for (int i = 0; i < 8; i++) std::printf(" %.17g", ks.lfo[i]);
    std::printf("\n");
  }

  std::printf("\n== ORBITAL parity: 8-body ring, dt=1/480, 4800 steps, sampled every 48 ==\n");
  OrbitalField f;
  f.nodes = eightBodyRing();
  f.G = 0.025; f.eps = 0.06; f.ts = 1.0; f.damp = 0.0; f.wrap = false; f.com = true;
  f.dtPhys = 1.0 / 480.0; f.capSteps = 8;
  for (int step = 1; step <= 4800; step++) {
    f.step(f.dtPhys);
    if (step % 48 == 0) {
      std::printf("t=%4d body0 x=%.17g y=%.17g vx=%.17g vy=%.17g | body7 x=%.17g y=%.17g vx=%.17g vy=%.17g\n",
                  step, f.nodes[0].x, f.nodes[0].y, f.nodes[0].vx, f.nodes[0].vy,
                  f.nodes[7].x, f.nodes[7].y, f.nodes[7].vx, f.nodes[7].vy);
    }
  }
}

/* ---------------------------------------------------------------- benchmark
 * Two rates the brief asks for, PLUS the real modulation grid (see file
 * header): PER_SAMPLE, PER_TICK16 (literal ask), PER_GRAVGRID (~172 Hz,
 * kGravGridSeconds — the rate that actually matters). Each tick size is
 * recomputed per sample rate via lround(sr * seconds), exactly as
 * hypersaw_clap.cpp does (ADR-009: seconds, never a per-tick constant). */
struct RateSpec { const char *name; int samplesPerTick; };

int ticksAt(double sr, double seconds) { return std::max(1, (int)std::lround(sr * seconds)); }

struct Result { double nsPerCall; double pct; long long calls; };

// Times `totalSeconds` of simulated audio, min of 3 reps.
Result timeKuro(double sr, int n, double K, int samplesPerTick, double totalSeconds) {
  const long long totalSamples = (long long)std::llround(sr * totalSeconds);
  const long long calls = std::max<long long>(1, totalSamples / samplesPerTick);
  double best = 1e9;
  for (int rep = 0; rep < 3; rep++) {
    KuroSwarm ks(sr, n);
    ks.p.K = K;
    const auto t0 = Clock::now();
    for (long long c = 0; c < calls; c++) ks.advance(samplesPerTick);
    const auto t1 = Clock::now();
    // sink: read back state through a volatile so the whole loop cannot be
    // proven dead and elided.
    volatile double sink = ks.phase[0] + ks.lfo[1];
    (void)sink;
    best = std::min(best, ms(t0, t1));
  }
  const double audioS = (double)(calls * samplesPerTick) / sr;
  return {best * 1e6 / (double)calls, best / (audioS * 1000.0) * 100.0, calls};
}

Result timeOrbital(double sr, int nBodies, double dtPhys, int capSteps, int samplesPerTick, double totalSeconds) {
  const long long totalSamples = (long long)std::llround(sr * totalSeconds);
  const long long calls = std::max<long long>(1, totalSamples / samplesPerTick);
  double best = 1e9;
  long long droppedAcrossReps = 0;
  for (int rep = 0; rep < 3; rep++) {
    OrbitalField f;
    f.nodes = eightBodyRing();
    if ((int)f.nodes.size() > nBodies) f.nodes.resize(nBodies);
    f.G = 0.025; f.eps = 0.06; f.ts = 1.0; f.damp = 0.0; f.wrap = false; f.com = true;
    f.dtPhys = dtPhys; f.capSteps = capSteps;
    const auto t0 = Clock::now();
    for (long long c = 0; c < calls; c++) f.advance(sr, samplesPerTick);
    const auto t1 = Clock::now();
    volatile double sink = f.nodes[0].x + f.nodes.back().vy;
    (void)sink;
    droppedAcrossReps += f.droppedTicks;
    best = std::min(best, ms(t0, t1));
  }
  if (droppedAcrossReps) std::fprintf(stderr, "measure_modsources: WARNING — %lld capped ORBITAL ticks (steps silently dropped)\n", droppedAcrossReps);
  const double audioS = (double)(calls * samplesPerTick) / sr;
  return {best * 1e6 / (double)calls, best / (audioS * 1000.0) * 100.0, calls};
}

}  // namespace

int main(int argc, char **argv) {
  if (argc > 1 && std::strcmp(argv[1], "--parity") == 0) {
    runParityDump();
    return 0;
  }

  std::printf("measure_modsources — B236. Release build required for meaningful numbers");
#ifndef NDEBUG
  std::printf(" (this IS a Debug/-O0 build: numbers below are compiler noise, not cost).\n");
#else
  std::printf(" (this is a Release build).\n");
#endif

  const double calib = calibLoopMs(20000000);
  std::printf("\ncalibration control: fixed FLOP loop, min-of-3 = %.2f ms (must be >> 0; proves the\n"
              "timer sees real work and the optimiser has not deleted the benchmark loops below).\n\n", calib);

  const double kGravGridSeconds = 256.0 / 44100.0;   // src/swarm_core.h:136, ROADMAP B199
  const double srs[] = {44100.0, 48000.0, 96000.0};
  const double totalSeconds = 20.0;

  std::printf("== KURO LFO, n=8 (worst case per ADR-053's MAXK) ==\n");
  std::printf("| K (branch) | sample rate | rate | ns/call | %% of one core |\n");
  std::printf("|---|---|---|---|---|\n");
  double kuroWorstPct = 0;
  for (double K : {-0.7, 0.7}) {   // -: rank-lattice/splay (insertion sort); +: mean-field
    const char *branch = K < 0 ? "splay" : "mean-field";
    for (double sr : srs) {
      const std::vector<RateSpec> rates = {
          {"per-sample", 1},
          {"per-16-tick", 16},
          {"per-gravgrid(~172Hz)", ticksAt(sr, kGravGridSeconds)},
      };
      for (const auto &rs : rates) {
        const Result r = timeKuro(sr, 8, K, rs.samplesPerTick, totalSeconds);
        kuroWorstPct = std::max(kuroWorstPct, r.pct);
        std::printf("| %+.1f (%s) | %.0f Hz | %s (%d smp) | %.1f | %.4f%% |\n",
                    K, branch, sr, rs.name, rs.samplesPerTick, r.nsPerCall, r.pct);
      }
    }
  }

  std::printf("\n== ORBITAL, 8 bodies (max per SPEC-ORBITAL §3.1) ==\n");
  std::printf("| dt config | sample rate | rate | ns/call | %% of one core |\n");
  std::printf("|---|---|---|---|---|\n");
  double orbWorstPct = 0;
  // dt=1/960 (SPEC §5/§11, the plugin's OWN — finer, more expensive — choice;
  // cap 32 matches the spec's block-cap text). dt=1/480 (lab/reference value,
  // cap 8 matching the lab's advance()) is included as the lower bound the
  // parity dump above is checked against, so a reader sees both bracketed.
  struct DtCfg { const char *name; double dt; int cap; };
  const DtCfg dts[] = {{"1/960s (spec, shipped)", 1.0 / 960.0, 32}, {"1/480s (lab/reference)", 1.0 / 480.0, 8}};
  for (const auto &dc : dts) {
    for (double sr : srs) {
      const std::vector<RateSpec> rates = {
          {"per-sample", 1},
          {"per-16-tick", 16},
          {"per-gravgrid(~172Hz)", ticksAt(sr, kGravGridSeconds)},
      };
      for (const auto &rs : rates) {
        const Result r = timeOrbital(sr, 8, dc.dt, dc.cap, rs.samplesPerTick, totalSeconds);
        if (dc.dt < 1.0 / 900.0) orbWorstPct = std::max(orbWorstPct, r.pct);   // the shipped config only
        std::printf("| %s | %.0f Hz | %s (%d smp) | %.1f | %.4f%% |\n",
                    dc.name, sr, rs.name, rs.samplesPerTick, r.nsPerCall, r.pct);
      }
    }
  }

  // The engine's own recorded budget (tools/measure_cpu.cpp's audit rows,
  // 2026-09-18, Apple M3, 8 held notes — the SAME polyphony convention used
  // for the x8 comparison below): 1 osc n=32 = 7.00% of a core, 2 osc n=32 =
  // 14.15%. Quoted, not recomputed — a re-measurement is measure_cpu's job.
  std::printf("\n== comparison against the engine's recorded budget ==\n");
  std::printf("tools/measure_cpu.cpp audit (2026-09-18, 8 held notes): 1 osc n=32 = 7.00%% of a core; 2 osc n=32 = 14.15%%.\n");
  std::printf("KURO worst single instance: %.4f%% of a core; x8 (8-voice polyphony, same convention as measure_cpu): %.4f%%.\n",
              kuroWorstPct, kuroWorstPct * 8.0);
  std::printf("ORBITAL worst single instance (shipped 1/960s dt): %.4f%% of a core; x8: %.4f%%.\n",
              orbWorstPct, orbWorstPct * 8.0);
  std::printf("(x8 is a linear upper bound, per-voice worst case, matching SPEC-ORBITAL §2's opt-in per-voice\n"
              "scope; the spec's OWN default is one GLOBAL field, i.e. the single-instance row, not x8.)\n");

  return 0;
}
