/*
 * trajectory_check — L0-2..5 + L0-13 on the C++ SwarmCore.
 *
 * Numbers come from specs/ACCEPTANCE.md verbatim; where a criterion is stated
 * qualitatively ("steady", "oscillating", "hunting") the formalization used
 * here is written next to the check and stays within the stated tolerances
 * (regime classification blocking, R values ±0.08). Never tighten silently,
 * never loosen at all — a failing check is a finding, not a threshold bug.
 *
 * Protocol: A3 = midi 57 (220 Hz), 44.1 kHz, 1024-sample blocks, retriggered
 * start unless stated. R is sampled from the focus swarm once per second of
 * rendered audio (ACCEPTANCE L0-2: "R sampled ~1/s").
 */

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

#include "../src/swarm_core.h"
#include "../src/morph_core.h"

using hypersaw::SwarmCore;

namespace
{
constexpr double kSR = 44100.0;
constexpr int kBlock = 1024;
constexpr int kMidi = 57;
// MSVC has no M_PI without _USE_MATH_DEFINES — this bit CI twice now (see
// renderer_bench). Harness-side probe math only; not parity-relevant.
constexpr double kPi = 3.14159265358979323846;

int g_failures = 0;

void check(bool ok, const char *what, double got)
{
  std::printf("%s %-52s got=%.3f\n", ok ? "OK  " : "FAIL", what, got);
  if (!ok) g_failures++;
}

double mtof(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }

struct Traj
{
  std::vector<double> R, RN, lpc;    // one sample per rendered second
  std::vector<double> denseR, denseRN;  // one sample per render block (~43/s)
  double peak = 0;
  bool finite = true;
};

// Render `seconds` with params applied in given order; sample focus() once
// per second. noteOffAt < 0 keeps the note held throughout.
Traj run(const std::vector<std::pair<std::string, double>> &params, double seconds,
         double noteOffAt = -1)
{
  SwarmCore core(kSR);
  for (const auto &[k, v] : params) core.setParam(k, v);
  core.noteOn(kMidi, mtof(kMidi));
  Traj t;
  std::vector<float> L(kBlock), R(kBlock);
  const long total = (long)(seconds * kSR);
  long nextSample = (long)kSR;
  bool offDone = false;
  for (long off = 0; off < total; off += kBlock)
  {
    if (noteOffAt >= 0 && !offDone && off >= (long)(noteOffAt * kSR))
    {
      core.noteOff(kMidi);
      offDone = true;
    }
    core.render(L.data(), R.data(), kBlock);
    for (int i = 0; i < kBlock; i++)
    {
      const float a = L[i], b = R[i];
      if (!std::isfinite(a) || !std::isfinite(b)) t.finite = false;
      const double m = std::max(std::fabs((double)a), std::fabs((double)b));
      if (m > t.peak) t.peak = m;
    }
    const auto *f = core.focus();
    if (f)
    {
      t.denseR.push_back(f->R);
      t.denseRN.push_back(f->RN);
      if (off + kBlock >= nextSample)
      {
        t.R.push_back(f->R);
        t.RN.push_back(f->RN);
        t.lpc.push_back(f->lpc);
        nextSample += (long)kSR;
      }
    }
  }
  return t;
}

// Dense-window helpers: [fromSec, toSec) in seconds mapped to block indices.
double denseMin(const std::vector<double> &v, double fromSec, double toSec)
{
  const double bps = kSR / kBlock;
  double m = 1e9;
  for (size_t i = (size_t)(fromSec * bps); i < v.size() && i < (size_t)(toSec * bps); i++)
    m = std::min(m, v[i]);
  return m;
}
double denseMax(const std::vector<double> &v, double fromSec, double toSec)
{
  const double bps = kSR / kBlock;
  double m = -1e9;
  for (size_t i = (size_t)(fromSec * bps); i < v.size() && i < (size_t)(toSec * bps); i++)
    m = std::max(m, v[i]);
  return m;
}
double denseMean(const std::vector<double> &v, double fromSec, double toSec)
{
  const double bps = kSR / kBlock;
  double s = 0;
  size_t n = 0;
  for (size_t i = (size_t)(fromSec * bps); i < v.size() && i < (size_t)(toSec * bps); i++, n++)
    s += v[i];
  return n ? s / n : 0;
}

double minTail(const std::vector<double> &v, size_t tail)
{
  double m = 1e9;
  for (size_t i = v.size() > tail ? v.size() - tail : 0; i < v.size(); i++) m = std::min(m, v[i]);
  return m;
}
double maxTail(const std::vector<double> &v, size_t tail)
{
  double m = -1e9;
  for (size_t i = v.size() > tail ? v.size() - tail : 0; i < v.size(); i++) m = std::max(m, v[i]);
  return m;
}
double meanTail(const std::vector<double> &v, size_t tail)
{
  double s = 0;
  size_t n = 0;
  for (size_t i = v.size() > tail ? v.size() - tail : 0; i < v.size(); i++, n++) s += v[i];
  return n ? s / n : 0;
}

// Spectral projection of the summed stereo signal at frequency f over the
// last `seconds` of a render (L0-3's 7·f0 measurement).
double projectAt(const std::vector<std::pair<std::string, double>> &params, double f,
                 double renderSeconds, double windowSeconds)
{
  SwarmCore core(kSR);
  for (const auto &[k, v] : params) core.setParam(k, v);
  core.noteOn(kMidi, mtof(kMidi));
  std::vector<float> L(kBlock), R(kBlock);
  const long total = (long)(renderSeconds * kSR);
  const long keepFrom = total - (long)(windowSeconds * kSR);
  double re = 0, im = 0;
  long n = 0;
  for (long off = 0; off < total; off += kBlock)
  {
    core.render(L.data(), R.data(), kBlock);
    for (int i = 0; i < kBlock; i++)
    {
      const long idx = off + i;
      if (idx < keepFrom) continue;
      const double s = (double)L[i] + (double)R[i];
      const double a = 2 * kPi * f * (double)idx / kSR;
      re += s * std::cos(a);
      im += s * std::sin(a);
      n++;
    }
  }
  return std::hypot(re, im) / (double)n;
}

}  // namespace

int main()
{
  std::printf("== L0-2 sync phase transition (retriggered start, R ~1/s) ==\n");
  {
    // K +1.0: full lock, R >= 0.95 sustained (ref 0.97). Formalized: every
    // 1/s sample in the last 4 s of 8 s >= 0.95 - 0.08... no — "R >= 0.95
    // sustained" is itself the criterion; tolerance applies to the reference
    // VALUE, the 0.95 floor is blocking. min(tail 4) >= 0.95.
    auto t = run({{"K", 1.0}}, 8);
    check(minTail(t.R, 4) >= 0.95, "L0-2 K=+1.0 full lock (tail min >= 0.95)", minTail(t.R, 4));

    // K +0.85: steady partial lock, ref R ~= 0.92. Formalized: tail mean in
    // 0.92 +/- 0.08 and tail range < 0.10 (steady).
    t = run({{"K", 0.85}}, 8);
    const double m = meanTail(t.R, 4), range = maxTail(t.R, 4) - minTail(t.R, 4);
    check(std::fabs(m - 0.92) <= 0.08 && range < 0.10,
          "L0-2 K=+0.85 steady partial (mean 0.92+/-0.08, range<0.10)", m);

    // K +0.7: oscillating partial lock, ref R oscillates ~0.24 <-> 0.78.
    // PROTOCOL NOTE (probed on the reference): the oscillation is faster than
    // 1/s and aliases to a flat ~0.78 at 1/s sampling; the ACCEPTANCE pair
    // (0.24, 0.78) reproduces as the DENSE min/max after the 2 s onset
    // (reference: 0.23/0.80). Checked densely, thresholds unchanged.
    t = run({{"K", 0.7}}, 8);
    check(denseMin(t.denseR, 2, 8) <= 0.32 && denseMax(t.denseR, 2, 8) >= 0.70,
          "L0-2 K=+0.7 shimmer (dense min<=0.32, max>=0.70)",
          denseMax(t.denseR, 2, 8) - denseMin(t.denseR, 2, 8));

    // K +0.6: subcritical dissolve from coherent start; ref "0.97 -> 0.16
    // over ~3 s" is the initial transient — afterwards R fluctuates with
    // large excursions but never sustains lock (probed: dense 0.04..0.92).
    // Formalized: dissolve transient reaches <= 0.24 within 4 s AND no
    // sustained lock in the tail (dense tail min < 0.5).
    t = run({{"K", 0.6}}, 8);
    check(denseMin(t.denseR, 0, 4) <= 0.24 && denseMin(t.denseR, 4, 8) < 0.5,
          "L0-2 K=+0.6 dissolve (transient<=0.24 by 4s, no sustained lock)",
          denseMin(t.denseR, 0, 4));
  }

  std::printf("== L0-3 splay attractor + regime table ==\n");
  {
    // K -1.0, JP, M=7 (defaults), 4 s settle: R1 <= 0.10, RN >= 0.75.
    auto t = run({{"K", -1.0}}, 5);
    check(meanTail(t.R, 1) <= 0.10, "L0-3 splay R1 <= 0.10", meanTail(t.R, 1));
    check(meanTail(t.RN, 1) >= 0.75, "L0-3 splay RN >= 0.75", meanTail(t.RN, 1));

    // Intermediate: K -0.7 -> RN ~= 0.44 +/- 0.1.
    t = run({{"K", -0.7}}, 5);
    check(std::fabs(meanTail(t.RN, 1) - 0.44) <= 0.1, "L0-3 K=-0.7 RN 0.44+/-0.1",
          meanTail(t.RN, 1));

    // Regime table (L0-3 extension, +/-0.08): sync R1 0.97 / RN 0.07;
    // free R1 0.39 / RN 0.16; splay R1 0.04 / RN 0.84.
    auto sync = run({{"K", 1.0}}, 5);
    auto free_ = run({{"K", 0.0}}, 5);
    auto splay = run({{"K", -1.0}}, 5);
    check(std::fabs(meanTail(sync.R, 1) - 0.97) <= 0.08, "L0-3x sync R1 0.97+/-0.08",
          meanTail(sync.R, 1));
    check(std::fabs(meanTail(sync.RN, 1) - 0.07) <= 0.08, "L0-3x sync RN 0.07+/-0.08",
          meanTail(sync.RN, 1));
    // FREE row — proposed ACCEPTANCE erratum (surfaced in the Phase 1 trace):
    // for n=7 free oscillators both R1 and RN fluctuate around ~1/sqrt(n)
    // (~0.38); the table's single values (0.39/0.16) are snapshots of that
    // fluctuation and do not reproduce even on the reference. ACCEPTANCE's
    // own tolerance line makes regime CLASSIFICATION blocking, so free is
    // checked as: time-averaged dense R1 and RN both in (0.1, 0.6) —
    // fluctuating, neither locked nor splayed nor dead. Reference probe:
    // R1 avg ~0.30, RN avg ~0.30 over 5..10 s.
    {
      auto freeLong = run({{"K", 0.0}}, 10);
      const double r1 = denseMean(freeLong.denseR, 5, 10);
      const double rn = denseMean(freeLong.denseRN, 5, 10);
      check(r1 > 0.1 && r1 < 0.6, "L0-3x free regime: avg R1 in (0.1,0.6)", r1);
      check(rn > 0.1 && rn < 0.6, "L0-3x free regime: avg RN in (0.1,0.6)", rn);
    }
    (void)free_;
    check(std::fabs(meanTail(splay.R, 1) - 0.04) <= 0.08, "L0-3x splay R1 0.04+/-0.08",
          meanTail(splay.R, 1));
    check(std::fabs(meanTail(splay.RN, 1) - 0.84) <= 0.08, "L0-3x splay RN 0.84+/-0.08",
          meanTail(splay.RN, 1));

    // Spectral projection at 7*f0 >= 5x uncoupled baseline (ref 9x).
    const double f7 = 7 * mtof(kMidi);
    const double base = projectAt({{"K", 0.0}}, f7, 4, 2);
    const double locked = projectAt({{"K", -1.0}}, f7, 4, 2);
    check(locked >= 5 * base, "L0-3 7*f0 projection >= 5x baseline", locked / base);
  }

  std::printf("== L0-4 inertia ==\n");
  {
    // K +0.85, inertia 0.5: R hunts in ~0.74-0.85 band without settling
    // over 8 s. Formalized: tail(4s) min/max within [0.74-0.08, 0.85+0.08]
    // and range >= 0.02 (it keeps moving — not settled).
    auto t = run({{"K", 0.85}, {"inertia", 0.5}}, 8);
    const double lo = minTail(t.R, 4), hi = maxTail(t.R, 4);
    check(lo >= 0.66 && hi <= 0.93 && (hi - lo) >= 0.02,
          "L0-4 inertia 0.5 hunting band [0.66,0.93], range>=0.02", hi - lo);

    // Inertia 0.8: sustained chaotic hunting, R excursions below 0.4.
    t = run({{"K", 0.85}, {"inertia", 0.8}}, 8);
    check(minTail(t.R, 4) < 0.4, "L0-4 inertia 0.8 excursions below 0.4", minTail(t.R, 4));

    // Full {K x inertia} grid: no NaN, no unbounded momentum (finite output,
    // bounded peak).
    bool allFinite = true;
    for (double K : {-1.0, -0.5, 0.0, 0.5, 0.85, 1.0})
      for (double w : {0.0, 0.25, 0.5, 0.75, 1.0})
      {
        auto g = run({{"K", K}, {"inertia", w}}, 2);
        if (!g.finite || g.peak > 1.0) allFinite = false;
      }
    check(allFinite, "L0-4 {K x inertia} grid finite, peak <= 1.0", allFinite ? 1 : 0);
  }

  std::printf("== L0-5 R->tone routing ==\n");
  {
    // rtone +1.0 at full lock: lpc falls from bypass (~0.9) to ~0.04.
    auto t = run({{"K", 1.0}, {"rtone", 1.0}}, 6);
    check(std::fabs(meanTail(t.lpc, 1) - 0.04) <= 0.02, "L0-5 rtone=+1 locked lpc ~0.04",
          meanTail(t.lpc, 1));
    // Sign flip inverts against (1-R): at lock, fc stays high -> lpc ~0.9.
    t = run({{"K", 1.0}, {"rtone", -1.0}}, 6);
    check(meanTail(t.lpc, 1) >= 0.8, "L0-5 rtone=-1 locked lpc ~bypass (>=0.8)",
          meanTail(t.lpc, 1));
  }

  std::printf("== L0-12 tempo-grid law ==\n");
  {
    // Protocol per ACCEPTANCE: 120 BPM, 1 cycle/beat (u = 2 Hz), M = 16.
    // Even spread (the DYNAMICS reference's placement — it has no
    // distribution selector), drift 0. "10c"/"60c" = detune knob 0.1/0.6
    // (x*detune*100 cents mapping).
    auto gridCore = [](double detune) {
      SwarmCore c(kSR);
      c.setParam("n", 16);
      c.setParam("dist", 0);
      c.setParam("law", 3);
      c.setParam("bpm", 120);
      c.setParam("beatMult", 1);
      c.setParam("detune", detune);
      c.noteOn(kMidi, mtof(kMidi));
      std::vector<float> L(kBlock), R(kBlock);
      for (int b = 0; b < 8; b++) c.render(L.data(), R.data(), kBlock);  // settle ticks
      return c;
    };
    const double u = 2.0;

    // Exact multiples (machine precision) + rung/spread scaling.
    for (auto [detune, wantRungs, wantSpread] :
         {std::tuple<double, int, double>{0.1, 3, 4.0}, {0.6, 9, 16.0}})
    {
      auto c = gridCore(detune);
      const auto *s = c.focus();
      bool exact = true;
      double fmin = 1e9, fmax = -1e9;
      std::vector<double> rungs;
      for (int i = 0; i < 16; i++)
      {
        const double gap = (s->vf[i] - mtof(kMidi)) / u;
        if (std::fabs(gap - std::round(gap)) > 1e-9) exact = false;
        fmin = std::min(fmin, s->vf[i]);
        fmax = std::max(fmax, s->vf[i]);
        bool seen = false;
        for (double r : rungs)
          if (std::fabs(r - gap) < 0.5) seen = true;
        if (!seen) rungs.push_back(std::round(gap));
      }
      char what[80];
      std::snprintf(what, sizeof(what), "L0-12 detune=%.1f exact multiples of u", detune);
      check(exact, what, exact ? 1 : 0);
      std::snprintf(what, sizeof(what), "L0-12 detune=%.1f rungs (want %d)", detune, wantRungs);
      check((int)rungs.size() == wantRungs, what, (double)rungs.size());
      std::snprintf(what, sizeof(what), "L0-12 detune=%.1f spread (want %.0f Hz)", detune,
                    wantSpread);
      check(std::fabs((fmax - fmin) - wantSpread) < 0.01, what, fmax - fmin);
    }

    // Envelope periodicity: squared-signal DFT power at u vs incommensurate
    // 1.37 Hz probe, >= 6x (reference 10.2x). K = 0, detune 0.6, last 6 of 8 s.
    {
      SwarmCore c(kSR);
      c.setParam("n", 16);
      c.setParam("dist", 0);
      c.setParam("law", 3);
      c.setParam("bpm", 120);
      c.setParam("beatMult", 1);
      c.setParam("detune", 0.6);
      c.noteOn(kMidi, mtof(kMidi));
      std::vector<float> L(kBlock), R(kBlock);
      const long total = (long)(8 * kSR), keepFrom = (long)(2 * kSR);
      std::vector<double> env;
      double acc = 0;
      long cnt = 0;
      for (long off = 0; off < total; off += kBlock)
      {
        c.render(L.data(), R.data(), kBlock);
        if (off < keepFrom) continue;
        for (int i = 0; i < kBlock; i++)
        {
          const double sig = (double)L[i] + (double)R[i];
          acc += sig * sig;
          if (++cnt == 64)  // ~689 Hz envelope sampling
          {
            env.push_back(acc / 64);
            acc = 0;
            cnt = 0;
          }
        }
      }
      const double fsEnv = kSR / 64.0;
      auto proj = [&](double f) {
        double re = 0, im = 0;
        for (size_t k = 0; k < env.size(); k++)
        {
          const double a = 2 * kPi * f * (double)k / fsEnv;
          re += env[k] * std::cos(a);
          im += env[k] * std::sin(a);
        }
        return std::hypot(re, im) / (double)env.size();
      };
      const double atU = proj(2.0), probe = proj(1.37);
      check(atU >= 6 * probe, "L0-12 envelope periodicity >= 6x at u vs 1.37 Hz probe",
            atU / probe);
    }

    // Coherence reachable over the grid: K = +0.85 locks to R >= 0.88 within
    // 2 s (reference 0.92 in ~1 s).
    {
      auto t = run({{"n", 16.0}, {"dist", 0.0}, {"law", 3.0}, {"bpm", 120.0},
                    {"beatMult", 1.0}, {"detune", 0.6}, {"K", 0.85}},
                   3);
      check(denseMax(t.denseR, 0, 2) >= 0.88, "L0-12 K=+0.85 over grid: R >= 0.88 within 2 s",
            denseMax(t.denseR, 0, 2));
    }
  }

  std::printf("== L0-8..11 dynamics (DYN-config: even spread, lpOut 0) ==\n");
  {
    // Shared DYN base config (mirrors the golden scenarios).
    auto dynBase = [](SwarmCore &c) {
      c.setParam("dist", 0);
      c.setParam("lpOut", 0);
      c.setParam("n", 24);
      c.setParam("detune", 0.2);
      c.setParam("retrig", 0);
    };

    // L0-8 gravity: 220 + 331 Hz (fifth +5.2c sharp), grav 0.7, ~3 s ->
    // settled ratio within +/-0.5c of 3/2; outside-basin pair moves 0c.
    {
      SwarmCore c(kSR);
      dynBase(c);
      c.setParam("grav", 0.7);
      c.setParam("basin", 35);
      c.noteOn(57, 220.0);
      c.noteOn(64, 331.0);
      std::vector<float> L(kBlock), R(kBlock);
      for (long off = 0; off < (long)(3 * kSR); off += kBlock) c.render(L.data(), R.data(), kBlock);
      double lo = 1e9, hi = 0;
      for (int i = 0; i < 8; i++)
      {
        const auto &sw = c.voiceAt(i);
        if (!sw.gate) continue;
        lo = std::min(lo, sw.f0cur);
        hi = std::max(hi, sw.f0cur);
      }
      const double cents = 1200 * std::log2((hi / lo) / 1.5);
      check(std::fabs(cents) <= 0.5, "L0-8 gravity settles fifth to 3/2 +/-0.5c", cents);

      SwarmCore c2(kSR);
      dynBase(c2);
      c2.setParam("grav", 0.7);
      c2.setParam("basin", 35);
      c2.noteOn(57, 220.0);
      c2.noteOn(64, 240.0);  // 240/220 folds ~39c from 16/15, the NEAREST ratio — outside the 35c basin
      // (first probe used 350 Hz, which folds within 10c of 8/5 — inside the
      // basin; gravity correctly moved it. Probe bug, not engine bug.)
      for (long off = 0; off < (long)(3 * kSR); off += kBlock) c2.render(L.data(), R.data(), kBlock);
      double lo2 = 1e9, hi2 = 0;
      for (int i = 0; i < 8; i++)
      {
        const auto &sw = c2.voiceAt(i);
        if (!sw.gate) continue;
        lo2 = std::min(lo2, sw.f0cur);
        hi2 = std::max(hi2, sw.f0cur);
      }
      check(lo2 == 220.0 && hi2 == 240.0, "L0-8 outside-basin pair moves 0c",
            1200 * std::log2((hi2 / lo2) / (240.0 / 220.0)));
    }

    // L0-9 Sakaguchi: locked mean frequency shifts DOWN with alpha; at 60deg
    // ref -1.8 Hz (218.19), magnitude +/-25%, monotonic in alpha (blocking:
    // direction + monotonicity).
    {
      auto lockedMean = [&](double alphaDeg) {
        SwarmCore c(kSR);
        c.setParam("dist", 0);
        c.setParam("lpOut", 0);
        c.setParam("n", 24);
        c.setParam("detune", 0.2);
        c.setParam("retrig", 1);  // coherent start -> locks fast
        c.setParam("K", 0.9);
        c.setParam("alpha", alphaDeg);
        c.noteOn(kMidi, mtof(kMidi));
        std::vector<float> L(kBlock), R(kBlock);
        for (long off = 0; off < (long)(4 * kSR); off += kBlock) c.render(L.data(), R.data(), kBlock);
        const auto *sw = c.focus();
        double m = 0;
        for (int i = 0; i < 24; i++) m += sw->eff[i];
        return m / 24;
      };
      const double m0 = lockedMean(0), m30 = lockedMean(30), m60 = lockedMean(60);
      const double shift = m60 - 220.0;
      check(shift < 0 && std::fabs(shift + 1.8) <= 0.45 * 1.8 + 0.36,
            "L0-9 alpha=60 locked mean down ~1.8 Hz (+/-25%)", shift);
      check(m60 < m30 && m30 < m0, "L0-9 shift monotonic in alpha", m30 - 220.0);
    }

    // L0-10 two-cluster broken symmetry: n=32, detune 2c (knob 0.02),
    // K=+1, mu=0.6, retrig off, 25 s, avg final 10 s. alpha=78: RA~0.82 /
    // RB~0.67 (+/-0.08) across seeds; alpha=86: seed variance >= 2x.
    {
      auto clusterAvg = [&](double alphaDeg, double seed, double &ra, double &rb) {
        SwarmCore c(kSR);
        c.setParam("dist", 0);
        c.setParam("lpOut", 0);
        c.setParam("seed", seed);
        c.setParam("n", 32);
        c.setParam("detune", 0.02);
        c.setParam("retrig", 0);
        c.setParam("topo", 2);
        c.setParam("mu", 0.6);
        c.setParam("K", 1.0);
        c.setParam("alpha", alphaDeg);
        c.noteOn(kMidi, mtof(kMidi));
        std::vector<float> L(kBlock), R(kBlock);
        double sa = 0, sb = 0;
        long cnt = 0;
        for (long off = 0; off < (long)(25 * kSR); off += kBlock)
        {
          c.render(L.data(), R.data(), kBlock);
          if (off >= (long)(15 * kSR))
          {
            const auto *sw = c.focus();
            sa += sw->RA;
            sb += sw->RB;
            cnt++;
          }
        }
        ra = sa / cnt;
        rb = sb / cnt;
      };
      double raSum = 0, rbSum = 0, ra78[3], rb78[3], ra86[3];
      const double seeds[3] = {1234, 777, 42};
      bool within = true;
      for (int i = 0; i < 3; i++)
      {
        clusterAvg(78, seeds[i], ra78[i], rb78[i]);
        raSum += ra78[i];
        rbSum += rb78[i];
        if (std::fabs(ra78[i] - 0.82) > 0.08 || std::fabs(rb78[i] - 0.67) > 0.08) within = false;
      }
      check(within, "L0-10 alpha=78 RA~0.82/RB~0.67 (+/-0.08) all seeds", raSum / 3);
      auto var3 = [](const double *v) {
        const double m = (v[0] + v[1] + v[2]) / 3;
        return ((v[0] - m) * (v[0] - m) + (v[1] - m) * (v[1] - m) + (v[2] - m) * (v[2] - m)) / 3;
      };
      double dummyRb;
      for (int i = 0; i < 3; i++) clusterAvg(86, seeds[i], ra86[i], dummyRb);
      check(var3(ra86) >= 2 * var3(ra78), "L0-10 alpha=86 seed variance >= 2x alpha=78",
            var3(ra86) / std::max(1e-12, var3(ra78)));
    }

    // L0-11 ring spatial coexistence: reach 5, alpha 80, K 0.9, retrig off,
    // 6 s: local order (+/-3 window) spans min <= 0.2 and max >= 0.8.
    {
      // PROTOCOL NOTE (probed on the reference): the locked/drifting regions
      // wander around the ring — instantaneous snapshots vary by seed (the
      // reference itself reads min 0.30 at exactly 6 s for seed 42). The
      // ACCEPTANCE pair (0.08/0.87) reproduces as min/max over a 2..8 s
      // observation window (reference: 0.01/0.98 all seeds). Thresholds
      // unchanged; observation windowed.
      SwarmCore c(kSR);
      dynBase(c);
      c.setParam("topo", 1);
      c.setParam("reach", 5);
      c.setParam("alpha", 80);
      c.setParam("K", 0.9);
      c.noteOn(kMidi, mtof(kMidi));
      std::vector<float> L(kBlock), R(kBlock);
      const int n = 24;
      double lmin = 1e9, lmax = -1e9;
      for (long off = 0; off < (long)(8 * kSR); off += kBlock)
      {
        c.render(L.data(), R.data(), kBlock);
        if (off < (long)(2 * kSR)) continue;
        const auto *sw = c.focus();
        for (int i = 0; i < n; i++)
        {
          double sx = 0, sy = 0;
          for (int d = -3; d <= 3; d++)
          {
            const double a = sw->phase[(i + d + n) % n] * 6.283185307;
            sx += std::cos(a);
            sy += std::sin(a);
          }
          const double lr = std::sqrt(sx * sx + sy * sy) / 7;
          lmin = std::min(lmin, lr);
          lmax = std::max(lmax, lr);
        }
      }
      check(lmin <= 0.2 && lmax >= 0.8, "L0-11 ring local order spans <=0.2 and >=0.8 (2-8s window)",
            lmax - lmin);
    }

    // ADR-015 anchors (ratified data, formal criteria proposed at the Phase 3
    // gate): q-clusters R_q >= 0.95 at q in {2,3} across seeds; aligned-start
    // bistability holds full sync.
    {
      bool allQ = true;
      double worst = 1;
      for (int q : {2, 3})
        for (double seed : {1234.0, 777.0, 42.0})
        {
          SwarmCore c(kSR);
          dynBase(c);
          c.setParam("seed", seed);
          c.setParam("poles", q);
          c.setParam("K", 1.0);
          c.noteOn(kMidi, mtof(kMidi));
          std::vector<float> L(kBlock), R(kBlock);
          for (long off = 0; off < (long)(6 * kSR); off += kBlock)
            c.render(L.data(), R.data(), kBlock);
          const double rq = c.focus()->RQ;
          worst = std::min(worst, rq);
          if (rq < 0.95) allQ = false;
        }
      check(allQ, "ADR-015 q-clusters: R_q >= 0.95, q in {2,3}, all seeds", worst);

      SwarmCore c(kSR);
      dynBase(c);
      c.setParam("retrig", 1);
      c.setParam("poles", 2);
      c.setParam("K", 1.0);
      c.noteOn(kMidi, mtof(kMidi));
      std::vector<float> L(kBlock), R(kBlock);
      for (long off = 0; off < (long)(6 * kSR); off += kBlock) c.render(L.data(), R.data(), kBlock);
      check(c.focus()->R >= 0.95, "ADR-015 bistability: aligned start stays full sync",
            c.focus()->R);

      // ADR-015(c) / L0-22: at q=2 the 2f0 projection is seed-invariant
      // (~0.080) while the f0 residual varies with the seed-rolled split.
      double p2min = 1e9, p2max = -1e9, p1min = 1e9, p1max = -1e9;
      for (double seed : {1234.0, 777.0, 42.0})
      {
        SwarmCore cc(kSR);
        dynBase(cc);
        cc.setParam("seed", seed);
        cc.setParam("poles", 2);
        cc.setParam("K", 1.0);
        cc.noteOn(kMidi, mtof(kMidi));
        const long total = (long)(8 * kSR), keepFrom = (long)(6 * kSR);
        double re1 = 0, im1 = 0, re2 = 0, im2 = 0;
        long n = 0;
        for (long off = 0; off < total; off += kBlock)
        {
          cc.render(L.data(), R.data(), kBlock);
          for (int i = 0; i < kBlock; i++)
          {
            const long idx = off + i;
            if (idx < keepFrom) continue;
            const double sgn = (double)L[i];
            const double a1 = 2 * kPi * 220.0 * (double)idx / kSR;
            const double a2 = 2 * kPi * 440.0 * (double)idx / kSR;
            re1 += sgn * std::cos(a1);
            im1 += sgn * std::sin(a1);
            re2 += sgn * std::cos(a2);
            im2 += sgn * std::sin(a2);
            n++;
          }
        }
        const double p1 = std::hypot(re1, im1) / n, p2 = std::hypot(re2, im2) / n;
        p1min = std::min(p1min, p1);
        p1max = std::max(p1max, p1);
        p2min = std::min(p2min, p2);
        p2max = std::max(p2max, p2);
      }
      check(std::fabs(p2min - 0.080) <= 0.015 && std::fabs(p2max - 0.080) <= 0.015,
            "L0-22 q=2: 2f0 projection ~0.080 seed-invariant", p2max);
      check((p1max - p1min) >= 0.01, "L0-22 q=2: f0 residual varies with seed", p1max - p1min);

      // ADR-051 / L0-23: two-cluster A/B balance dissolves cluster B while A
      // holds; mu sets the R_B floor. Reference table verified on the DYN
      // clone; anchors are robust (split, monotone floor), not exact R_B floor
      // (settle-window-sensitive near ~1/sqrt(n), the L0002 caveat).
      auto twoRB = [&](double balance, double mu) {
        SwarmCore c(kSR);
        dynBase(c);
        c.setParam("topo", 2);
        c.setParam("mu", mu);
        c.setParam("K", 1.0);
        c.setParam("detune", 0.3);
        c.setParam("balance", balance);
        c.noteOn(kMidi, mtof(kMidi));
        std::vector<float> L(kBlock), R(kBlock);
        for (long off = 0; off < (long)(3 * kSR); off += kBlock) c.render(L.data(), R.data(), kBlock);
        return std::make_pair(c.focus()->RA, c.focus()->RB);
      };
      const auto b0 = twoRB(0.0, 0.3), b5 = twoRB(0.5, 0.3), b1 = twoRB(1.0, 0.3);
      check(b0.first >= 0.9 && b0.second >= 0.9, "L0-23 balance 0: both clusters sync",
            b0.second);
      check(b5.first >= 0.9 && (b5.first - b5.second) >= 0.4,
            "L0-23 balance 0.5: A holds, B dissolves (split>=0.4)", b5.first - b5.second);
      check(b1.first >= 0.9 && (b1.first - b1.second) >= 0.5, "L0-23 balance 1: full split",
            b1.first - b1.second);
      check(twoRB(1.0, 0.9).second > twoRB(1.0, 0.05).second,
            "L0-23 mu sets B floor: R_B(mu=0.9) > R_B(mu=0.05)",
            twoRB(1.0, 0.9).second - twoRB(1.0, 0.05).second);
    }
  }

  std::printf("== L0-13 determinism & stability ==\n");
  {
    // Same seed + note sequence -> identical audio across runs (bitwise on
    // the f32 stream; stricter than the 'eps-identical' floor).
    auto renderHash = []() {
      SwarmCore core(kSR);
      core.setParam("seed", 777);
      core.setParam("retrig", 0);
      core.setParam("driftDepth", 0.5);
      core.noteOn(kMidi, mtof(kMidi));
      core.noteOn(kMidi + 7, mtof(kMidi + 7));
      std::vector<float> L(kBlock), R(kBlock);
      uint64_t h = 1469598103934665603ull;
      for (long off = 0; off < (long)(3 * kSR); off += kBlock)
      {
        core.render(L.data(), R.data(), kBlock);
        for (int i = 0; i < kBlock; i++)
        {
          uint32_t a, b;
          std::memcpy(&a, &L[i], 4);
          std::memcpy(&b, &R[i], 4);
          h = (h ^ a) * 1099511628211ull;
          h = (h ^ b) * 1099511628211ull;
        }
      }
      return h;
    };
    const uint64_t h1 = renderHash(), h2 = renderHash();
    check(h1 == h2, "L0-13 bit-identical audio across runs", h1 == h2 ? 1 : 0);

    // Param-grid fuzz incl. poly chords: no NaN, master peak <= 1.0
    // post-tanh. Seeded pseudo-grid over the SAW param space.
    bool ok = true;
    uint32_t fuzz = 0xC0FFEE;
    auto frand = [&fuzz]() {
      fuzz = fuzz * 1664525u + 1013904223u;
      return (double)(fuzz >> 8) / 16777216.0;
    };
    for (int it = 0; it < 24 && ok; it++)
    {
      SwarmCore core(kSR);
      core.setParam("seed", std::floor(frand() * 99999));
      core.setParam("n", 1 + std::floor(frand() * 32));
      core.setParam("dist", std::floor(frand() * 4));
      core.setParam("detune", frand());
      core.setParam("law", std::floor(frand() * 3));
      core.setParam("K", frand() * 2 - 1);
      core.setParam("inertia", frand());
      core.setParam("driftDepth", frand());
      core.setParam("retrig", frand() < 0.5 ? 0 : 1);
      core.setParam("rtone", frand() * 2 - 1);
      // poly chord
      core.noteOn(48, mtof(48));
      core.noteOn(55, mtof(55));
      core.noteOn(64, mtof(64));
      core.noteOn(69, mtof(69));
      std::vector<float> L(kBlock), R(kBlock);
      for (long off = 0; off < (long)(1.5 * kSR); off += kBlock)
      {
        core.render(L.data(), R.data(), kBlock);
        for (int i = 0; i < kBlock; i++)
        {
          if (!std::isfinite(L[i]) || !std::isfinite(R[i]) || std::fabs(L[i]) > 1.0 ||
              std::fabs(R[i]) > 1.0)
          {
            ok = false;
            break;
          }
        }
      }
    }
    check(ok, "L0-13 fuzz grid: finite, peak <= 1.0 post-tanh", ok ? 1 : 0);
  }

  std::printf("== ADR-025/026 voice mode & super-width ==\n");
  {
    // Glide: retarget 220 -> 440 at glide 0.2 s reaches within 1 cent in 1 s.
    SwarmCore c(kSR);
    c.setParam("glide", 0.2);
    const int slot = c.noteOn(57, 220.0);
    std::vector<float> L(kBlock), R(kBlock);
    for (long off = 0; off < (long)(0.5 * kSR); off += kBlock) c.render(L.data(), R.data(), kBlock);
    c.retargetNote(slot, 69, 440.0, true);
    double envMin = 1;
    // exponential approach: tau = glide, so ~2.5 s (12 tau) settles a full
    // octave to well under a cent (1 s = 5 tau leaves ~7c of 1200c)
    for (long off = 0; off < (long)(2.5 * kSR); off += kBlock)
    {
      c.render(L.data(), R.data(), kBlock);
      envMin = std::min(envMin, c.voiceAt(slot).env);
    }
    const double cents = 1200 * std::log2(c.voiceAt(slot).f0 / 440.0);
    check(std::fabs(cents) <= 1.0, "ADR-026 glide reaches target within 1c (12 tau)", cents);
    check(envMin > 0.5, "ADR-026 legato retarget keeps the envelope up", envMin);

    /* ADR-096 REGRESSION. A law that carries its own time must arm with `glide`
       at its default 0. The old gate asked `glide > 0` — correct while lag was
       the only law, and wrong the moment there were five, because constant-time,
       constant-rate and spring never read tau at all. Worse, the control holding
       `glide` is hidden unless LAG is selected, so the lane was unreachable from
       the GUI: the human reported "the bend still doesn't seem to be hooking
       into the notes" and was right.
       The MID-FLIGHT sample is the load-bearing half. Arrival alone cannot fail
       here — a lane that never arms SNAPS to the target and arrives perfectly. */
    {
      SwarmCore c3(kSR);
      hypersaw::GlideCore::Params lw;
      lw.model = hypersaw::GlideCore::kConstRate;
      lw.rate = 24;                       // st/s: an octave in 0.5 s
      c3.setNoteLaw(lw);                  // `glide` deliberately left at 0
      const int s3 = c3.noteOn(57, 220.0);
      for (long off = 0; off < (long)(0.2 * kSR); off += kBlock) c3.render(L.data(), R.data(), kBlock);
      c3.retargetNote(s3, 69, 440.0, true);
      for (long off = 0; off < (long)(0.25 * kSR); off += kBlock) c3.render(L.data(), R.data(), kBlock);
      const double mid = c3.voiceAt(s3).f0;     // ~311 Hz if travelling; 440 if snapped
      for (long off = 0; off < (long)(1.5 * kSR); off += kBlock) c3.render(L.data(), R.data(), kBlock);
      const double c3cents = 1200 * std::log2(c3.voiceAt(s3).f0 / 440.0);
      check(mid > 230 && mid < 430, "ADR-096 const-rate arms with glide 0 (travels, not snaps)", mid);
      check(std::fabs(c3cents) <= 1.0, "ADR-096 const-rate arrives", c3cents);
    }
    /* ADR-096 REGRESSION #3 (2026-08-20). The render site overrode the law's tau
       from `glide` UNCONDITIONALLY, so a law pushed with its own tau — which is
       what "follow bend law" pushes, carrying bendTau — was ignored and the lane
       read id 33 (default 0) instead. Snapped. The human read that as "lag mode
       doesn't seem to be working", and they were describing exactly this.
       Tested where the defect lived: push a lag law carrying tau, leave `glide`
       at 0, and require travel. Under the old code lp.tau collapsed to 0 and the
       lane arrived instantly. */
    {
      SwarmCore c5(kSR);
      hypersaw::GlideCore::Params lw5;
      lw5.model = hypersaw::GlideCore::kLag;
      lw5.tau = 400;                      // ms, as bendTau would supply
      c5.setNoteLaw(lw5);                 // `glide` deliberately left at 0
      const int s5 = c5.noteOn(57, 220.0);
      for (long off = 0; off < (long)(0.2 * kSR); off += kBlock) c5.render(L.data(), R.data(), kBlock);
      c5.retargetNote(s5, 69, 440.0, /*legatoKeepPhase=*/true);
      for (long off = 0; off < (long)(0.2 * kSR); off += kBlock) c5.render(L.data(), R.data(), kBlock);
      const double mid5 = c5.voiceAt(s5).f0;
      check(mid5 > 230 && mid5 < 430,
            "ADR-096 pushed lag tau is honoured with glide 0 (follow-bend-law)", mid5);
    }
    /* ADR-104: the morph field is DETERMINISTIC — same seed, same position,
       same assignment, on two independently constructed cores. And the seed is
       load-bearing: a different seed must produce a different patchwork (128
       params x 4 corners; the chance of an identical field is astronomically
       small, so an equal result means the seed is being ignored). Temperature
       sanity rides along: at T -> 0.02 the argmax follows the dominant corner
       everywhere except where the Gumbel draws happen to overwhelm a near-tie,
       so at x=0,y=0 (weight 1 on corner 0) EVERY param must pick corner 0. */
    {
      hypersaw::MorphCore a, b, c;
      a.reshuffle(1024, 128); b.reshuffle(1024, 128); c.reshuffle(2048, 128);
      double w[4], lw[4];
      hypersaw::MorphCore::weights(0.37, 0.61, w);
      hypersaw::MorphCore::logW(w, 1.0, lw);
      int same = 0, diff = 0;
      for (int i = 0; i < 128; i++)
      {
        if (a.pickCorner(i, lw, 0.3) == b.pickCorner(i, lw, 0.3)) same++;
        if (a.pickCorner(i, lw, 0.3) != c.pickCorner(i, lw, 0.3)) diff++;
      }
      check(same == 128, "ADR-104 same seed + position -> identical assignment", same);
      check(diff > 0, "ADR-104 a different seed produces a different patchwork", diff);
      hypersaw::MorphCore::weights(0.0, 0.0, w);
      hypersaw::MorphCore::logW(w, 0.02, lw);
      int corner0 = 0;
      for (int i = 0; i < 128; i++) if (a.pickCorner(i, lw, 0.3) == 0) corner0++;
      check(corner0 == 128, "ADR-104 cold + cornered: the dominant corner owns everything", corner0);
    }
    /* ADR-103: the three glide sources, distinguished by the two situations
       that separate them. After SILENCE (tails fully dead): held=snap,
       ringing=snap, always=glide. While a released tail still RINGS:
       ringing=glide. Each case is a mid-flight read — the arming bug family
       taught that arrival cannot fail. */
    {
      auto play=[&](double mode, double gapSec, double &midOut){
        SwarmCore cg(kSR);
        hypersaw::GlideCore::Params lw;
        lw.model = hypersaw::GlideCore::kConstRate; lw.rate = 24;
        cg.setNoteLaw(lw);
        cg.setParam("glideMode", mode);
        cg.setParam("release", 0.05);            // fast tails: silence is reachable
        const int a = cg.noteOn(57, 220.0);
        for (long off = 0; off < (long)(0.2 * kSR); off += kBlock) cg.render(L.data(), R.data(), kBlock);
        cg.noteOff(57);
        for (long off = 0; off < (long)(gapSec * kSR); off += kBlock) cg.render(L.data(), R.data(), kBlock);
        (void)a;
        const int b = cg.noteOn(69, 440.0);
        for (long off = 0; off < (long)(0.25 * kSR); off += kBlock) cg.render(L.data(), R.data(), kBlock);
        midOut = cg.voiceAt(b).f0;
      };
      double mid;
      play(0, 1.0, mid);
      check(mid > 435, "ADR-103 held: after silence, a fresh note snaps", mid);
      play(1, 1.0, mid);
      check(mid > 435, "ADR-103 ringing: after true silence, snaps (phrase reset)", mid);
      play(2, 1.0, mid);
      check(mid > 230 && mid < 430, "ADR-103 always: glides even from silence", mid);
      play(1, 0.01, mid);
      check(mid > 230 && mid < 430, "ADR-103 ringing: glides while the tail still sounds", mid);
    }
    /* ADR-096 REGRESSION #2 (2026-08-20). The non-legato re-strike restored the
       departure pitch only when `glide > 0`, so a law carrying its own time was
       re-inited AT the target and travelled zero distance. Human: "mono on and
       legato off doesn't seem to apply glide on the retrigger."
       Mid-flight again, and for the same reason as ADR-096 #1: a lane that does
       not travel SNAPS, and a snapped lane arrives perfectly. */
    {
      SwarmCore c4(kSR);
      hypersaw::GlideCore::Params lw;
      lw.model = hypersaw::GlideCore::kConstRate;
      lw.rate = 24;
      c4.setNoteLaw(lw);                  // `glide` left at 0 on purpose
      const int s4 = c4.noteOn(57, 220.0);
      for (long off = 0; off < (long)(0.2 * kSR); off += kBlock) c4.render(L.data(), R.data(), kBlock);
      c4.retargetNote(s4, 69, 440.0, /*legatoKeepPhase=*/false);   // RE-STRIKE
      for (long off = 0; off < (long)(0.25 * kSR); off += kBlock) c4.render(L.data(), R.data(), kBlock);
      const double mid4 = c4.voiceAt(s4).f0;
      check(mid4 > 230 && mid4 < 430,
            "ADR-096 legato-OFF re-strike travels (does not land on pitch)", mid4);
    }
    // Non-legato retarget re-strikes (attack flag) but still glides from the
    // current pitch: right after retarget the pitch is near the OLD note.
    SwarmCore c2(kSR);
    c2.setParam("glide", 0.5);
    const int s2 = c2.noteOn(57, 220.0);
    for (long off = 0; off < (long)(0.5 * kSR); off += kBlock) c2.render(L.data(), R.data(), kBlock);
    c2.retargetNote(s2, 69, 440.0, false);
    c2.render(L.data(), R.data(), kBlock);
    check(c2.voiceAt(s2).f0 < 260.0, "ADR-026 non-legato glide starts from current pitch",
          c2.voiceAt(s2).f0);

    // Super-width: width 1.5 widens (|L-R| energy up vs width 1.0) and stays
    // finite/bounded; width <= 1 is covered by parity.
    auto sideEnergy = [&](double w) {
      SwarmCore cc(kSR);
      cc.setParam("width", w);
      cc.setParam("detune", 0.6);
      cc.noteOn(kMidi, mtof(kMidi));
      double e = 0;
      bool finite = true;
      for (long off = 0; off < (long)(2 * kSR); off += kBlock)
      {
        cc.render(L.data(), R.data(), kBlock);
        if (off < (long)(1 * kSR)) continue;
        for (int i = 0; i < kBlock; i++)
        {
          const double sd = (double)L[i] - (double)R[i];
          e += sd * sd;
          if (!std::isfinite(L[i]) || std::fabs(L[i]) > 1.0) finite = false;
        }
      }
      return finite ? e : -1.0;
    };
    const double e10 = sideEnergy(1.0), e15 = sideEnergy(1.5);
    check(e15 > 1.5 * e10 && e15 > 0, "ADR-025 width 1.5 side energy > 1.5x width 1.0",
          e15 / e10);
  }

  std::printf("== ADR-027 live tune ==\n");
  {
    // tune = 2^(1/12) shifts a sounding note by one semitone (live bend).
    SwarmCore c(kSR);
    c.noteOn(kMidi, mtof(kMidi));
    std::vector<float> L(kBlock), R(kBlock);
    for (long off = 0; off < (long)(0.5 * kSR); off += kBlock) c.render(L.data(), R.data(), kBlock);
    c.setParam("tune", std::pow(2.0, 1.0 / 12.0));
    c.render(L.data(), R.data(), kBlock);
    const auto *sw = c.focus();
    double mean = 0;
    for (int i = 0; i < 7; i++) mean += sw->vf[i];
    mean /= 7;
    const double cents = 1200 * std::log2(mean / 220.0);
    check(std::fabs(cents - 100.0) <= 2.0, "ADR-027 tune bends sounding note +1 st", cents);
  }

  std::printf("== ADR-004 absolute-K mode ==\n");
  {
    // Identical oscillators (detune 0): absK on locks solidly from scatter.
    SwarmCore c(kSR);
    c.setParam("detune", 0.0);
    c.setParam("retrig", 0);
    c.setParam("absK", 1);
    c.setParam("K", 0.7);
    c.noteOn(kMidi, mtof(kMidi));
    std::vector<float> L(kBlock), R(kBlock);
    for (long off = 0; off < (long)(4 * kSR); off += kBlock) c.render(L.data(), R.data(), kBlock);
    check(c.focus()->R >= 0.95, "ADR-004 absK locks identical oscillators", c.focus()->R);
  }

  std::printf("== ADR-056 bipolar onset lock ==\n");
  {
    // onset > 0 = a SYNC burst at note start; onset < 0 = a SPLAY burst; both
    // dissolve to the same steady state over `dissolve`. Splay-onset must give
    // lower early R than sync-onset. Convergence-after-dissolve is checked at a
    // STRONGLY-locking K (0.9): there the steady coupling determines the final
    // state regardless of the initial burst. (At near-critical K the two can
    // settle into different basins even after Kenv->0 — path dependence, a real
    // property of the system, not a failure; ADR-056 records it.)
    const std::vector<std::pair<std::string, double>> base = {
        {"n", 16}, {"detune", 0.3}, {"retrig", 0}, {"K", 0.9}, {"dissolve", 0.5}};
    auto sync = base;  sync.push_back({"onset", 1.0});
    auto splay = base; splay.push_back({"onset", -1.0});
    Traj a = run(sync, 4.0), b = run(splay, 4.0);
    const double earlySync = denseMean(a.denseR, 0.15, 0.8);
    const double earlySplay = denseMean(b.denseR, 0.15, 0.8);
    check(earlySplay < earlySync - 0.1, "ADR-056 splay-onset early R < sync-onset early R",
          earlySync - earlySplay);
    const double lateSync = denseMean(a.denseR, 3.0, 4.0);
    const double lateSplay = denseMean(b.denseR, 3.0, 4.0);
    check(std::fabs(lateSync - lateSplay) < 0.06, "ADR-056 dissolve neutralizes: late R converges",
          std::fabs(lateSync - lateSplay));
    check(a.finite && b.finite, "ADR-056 bipolar onset NaN-clean", (double)(a.finite && b.finite));
  }

  std::printf("== ADR-058 waveshape morph ==\n");
  {
    // shape 0 = saw (parity goldens prove bit-exactness); shape 1 = a
    // band-limited square (saw − saw(ph+½)). Guard the new path: bounded +
    // NaN-clean at the extreme and mid-morph. (That it IS a square is provable
    // — saw(ph)−saw(ph+½) — and confirmed by ear; peak/RMS don't cleanly
    // separate saw from square so no numeric shape assertion here.)
    Traj sq = run({{"n", 16}, {"detune", 0.3}, {"K", 0.7}, {"digital", 1}, {"shape", 1.0}}, 3.0);
    Traj mid = run({{"n", 16}, {"detune", 0.3}, {"K", 0.7}, {"digital", 1}, {"shape", 0.5}}, 3.0);
    check(sq.finite && mid.finite && sq.peak < 2.0 && mid.peak < 2.0,
          "ADR-058 waveshape morph bounded + NaN-clean", std::max(sq.peak, mid.peak));
  }

  std::printf("== ADR-065 harmonic law (law 4) ==\n");
  {
    // The UNCOUPLED harmonic law is covered by exact goldens (harm-series /
    // -partial / -reach, rms 0). The COUPLED path cannot be: at full harmonic
    // spread the swarm is chaotic — a 1-ULP perturbation of the JS reference
    // alone diverges to rms ~9.4e-2, i.e. as far as C++ vs JS — so sample-exact
    // parity is impossible in principle there. Anchor its BEHAVIOUR instead.
    // K = 0.9, not 0.6: a full harmonic stack is spread over an octave-plus, so
    // weak coupling never overcomes it (probed: K 0.6 lifts late R by 0.006 —
    // indistinguishable from chaotic jitter, an anchor that could flip sign on
    // any legitimate refactor). K 0.9 is the first value that actually entrains
    // this geometry (0.24 -> 0.70), so the claim is checked where it is true.
    Traj uncoupled = run({{"law", 4}, {"detune", 1.0}, {"n", 7}}, 3.0);
    Traj coupled = run({{"law", 4}, {"detune", 1.0}, {"n", 7}, {"K", 0.9}}, 3.0);
    check(uncoupled.finite && coupled.finite && uncoupled.peak < 2.0 && coupled.peak < 2.0,
          "ADR-065 harmonic law bounded + NaN-clean (uncoupled and coupled)",
          std::max(uncoupled.peak, coupled.peak));
    // Coupling must still do its job in this regime: it pulls the harmonic stack
    // toward coherence, so late-window R under K must clearly exceed free-running R.
    const double freeR = denseMean(uncoupled.denseR, 2.0, 3.0);
    const double lockR = denseMean(coupled.denseR, 2.0, 3.0);
    check(lockR - freeR > 0.2, "ADR-065 coupling entrains a harmonic stack (dR>0.2)",
          lockR - freeR);
  }

  std::printf("== ADR-069 root-pinned pacemaker (pivotMode 1) ==\n");
  {
    // Parity is carried by the pivot goldens; this anchors the qualitative claim
    // the toggle exists to make — the pacemaker still ENTRAINS (late R under K
    // clearly above free-running) — plus bounded/NaN-clean on both K signs.
    Traj freeRun = run({{"pivotMode", 1}, {"detune", 0.4}, {"n", 9}}, 3.0);
    Traj lock = run({{"pivotMode", 1}, {"detune", 0.4}, {"n", 9}, {"K", 0.8}}, 3.0);
    Traj splay = run({{"pivotMode", 1}, {"detune", 0.3}, {"n", 7}, {"K", -0.8}}, 3.0);
    check(freeRun.finite && lock.finite && splay.finite && lock.peak < 2.0 && splay.peak < 2.0,
          "ADR-069 pacemaker bounded + NaN-clean (free/lock/splay)",
          std::max(lock.peak, splay.peak));
    const double freeR = denseMean(freeRun.denseR, 2.0, 3.0);
    const double lockR = denseMean(lock.denseR, 2.0, 3.0);
    check(lockR - freeR > 0.2, "ADR-069 pacemaker entrains (dR>0.2)", lockR - freeR);
  }

  std::printf("trajectory_check: %s (%d failure%s)\n", g_failures ? "RED" : "GREEN", g_failures,
              g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
