/*
 * force_check — Track E0 oracle: forcecore reproduces the effect labs'
 * population trajectories seed-for-seed, and the trajectories satisfy the
 * population halves of ACCEPTANCE L0-14/15/17/19/20.
 *
 * Two layers per scenario:
 *  1. TRAJECTORY: v[] at golden checkpoints (ticks 0/1/10/100/1000/22050)
 *     matches the Node-run JS labs within 1e-9 log2 units (< a micro-cent;
 *     population math is Float64 end to end so the only slack needed is
 *     transcendental-ulp divergence between v8 and libm).
 *  2. CRITERIA: the acceptance statistics, computed from the C++ state with
 *     the probed protocol (drift OFF — the reference numbers encode a
 *     drift-off measurement; see the E0 trace).
 *
 * Setup mirrors the labs' state as a pure function of final params (the
 * generator applies 'seed' last, so pre-run state == rebuild(final params)
 * + reset-to-home; no call-sequence replay needed).
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>

#include "../src/force_core.h"

using namespace forcecore;

namespace
{

int g_failures = 0;
void check(bool ok, const char *fmt, double got)
{
  std::printf("%s ", ok ? "OK  " : "FAIL");
  std::printf(fmt, got);
  std::printf("\n");
  if (!ok) g_failures++;
}

constexpr double kSR = 44100;
constexpr int kTicks = 22050;  // 8 s
constexpr double kDt = 16.0 / kSR;

struct Scenario
{
  const char *name;
  char lab;  // 'F' FilterLab · 'P' PhaserLab · 'T' TimeLab
  int mode = 0, nb = 0, dist = 0, seed = 1234;
  double K = 0, inertia = 0, driftDepth = 0, driftRate = 0.4, grav = 0, basin = 45, bpm = 120;
};

// Mirrors gen_force_goldens.mjs SCENARIOS — same table, same order.
const Scenario kScenarios[] = {
    {"filter-free", 'F', 0, 16, 0},
    {"filter-sync", 'F', 0, 16, 0, 1234, 1},
    {"filter-splay", 'F', 0, 16, 0, 1234, -1},
    {"filter-grav", 'F', 0, 16, 0, 1234, 0, 0, 0, 0.4, 0.8},
    {"filter-antigrav", 'F', 0, 16, 0, 1234, 0, 0, 0, 0.4, -0.8},
    {"filter-gauss-drift", 'F', 0, 16, 1, 1234, 1, 0, 40},
    {"filter-cauchy-777", 'F', 0, 16, 2, 777, -1, 0, 40},
    {"filter-inertia", 'F', 0, 16, 0, 1234, 1, 0.7},
    {"phaser-sync", 'P', 0, 6, 0, 1234, 1},
    {"phaser-splay", 'P', 0, 6, 0, 1234, -1},
    {"echo-free", 'T', 0, 8, 1, 1234, 0, 0, 0, 0.3, 0, 50},
    {"echo-sync", 'T', 0, 8, 1, 1234, 1, 0, 0, 0.3, 0, 50},
    {"echo-splay", 'T', 0, 8, 1, 1234, -1, 0, 0, 0.3, 0, 50},
    {"echo-grav", 'T', 0, 8, 1, 1234, 0, 0, 0, 0.3, 0.8, 50},
    {"echo-inertia", 'T', 0, 8, 1, 1234, 1, 0.7, 0, 0.3, 0, 50},
    {"echo-drift-42", 'T', 0, 8, 1, 42, 0.5, 0, 60, 0.3, 0, 50},
    {"room-symp", 'T', 1, 8, 1, 1234, 0, 0, 0, 0.3, 0.9, 80},
};

struct Setup
{
  Pop pop;
  Params params;
  Profile prof;
  Attractor att;
};

Setup build(const Scenario &sc)
{
  Setup u;
  u.params = {sc.K, sc.inertia, sc.driftDepth, sc.driftRate, sc.grav, sc.basin};
  buildOffsets(u.pop, sc.dist, sc.nb, sc.seed);
  if (sc.lab == 'F' || sc.lab == 'P')
  {
    const double hzFloor = sc.lab == 'F' ? 40 : 60;
    u.prof = sc.lab == 'F' ? kFilterProfile : kPhaserProfile;
    placeFrequencyTargets(u.pop, 0, 9.64, 0.5, 110, hzFloor);
    u.att = {sc.grav != 0 ? 1 : 0, 110, 0.5};
  }
  else
  {
    u.prof = timeProfile(sc.mode);
    placeTimeTargets(u.pop, sc.mode, 0.55, 0.6);
    u.att = sc.mode == 0 ? Attractor{sc.grav != 0 ? 2 : 0, 110, 60 / sc.bpm}
                         : Attractor{sc.grav != 0 ? 3 : 0, 110, 60 / sc.bpm};
  }
  resetToHome(u.pop);
  return u;
}

/* ---- stats (probe-identical formulas) ---- */
double sigmaOf(const double *v, int n)
{
  double m = 0;
  for (int i = 0; i < n; i++) m += v[i];
  m /= n;
  double s = 0;
  for (int i = 0; i < n; i++)
  {
    const double d = v[i] - m;
    s += d * d;
  }
  return std::sqrt(s / n);
}
double gapCVOf(const double *v, int n)
{
  std::vector<double> s(v, v + n);
  std::sort(s.begin(), s.end());
  double gm = 0;
  for (int i = 1; i < n; i++) gm += s[i] - s[i - 1];
  gm /= (n - 1);
  double gv = 0;
  for (int i = 1; i < n; i++)
  {
    const double d = (s[i] - s[i - 1]) - gm;
    gv += d * d;
  }
  return std::sqrt(gv / (n - 1)) / gm;
}
struct GravStat
{
  double mean;  // mean |cents| over in-basin members (-1 if none)
  int cnt, within40;
};
GravStat gravStat(const Pop &s, const Attractor &att, double basin)
{
  double acc = 0;
  int cnt = 0, w40 = 0;
  for (int i = 0; i < s.n; i++)
  {
    const double tgt = attractorTarget(att, s.v[i]);
    if (std::isnan(tgt)) continue;
    const double errC = std::fabs(1200 * (s.v[i] - tgt));
    if (errC < 40) w40++;
    if (errC < basin)
    {
      acc += errC;
      cnt++;
    }
  }
  return {cnt ? acc / cnt : -1, cnt, w40};
}

/* ---- golden parsing ---- */
struct Checkpoint
{
  int tick;
  std::vector<double> v;
};
bool loadGolden(const std::string &path, std::vector<Checkpoint> &out)
{
  std::ifstream f(path);
  if (!f) return false;
  std::string line;
  Checkpoint cur;
  cur.tick = -1;
  while (std::getline(f, line))
  {
    if (line.rfind("tick ", 0) == 0)
    {
      if (cur.tick >= 0) out.push_back(cur);
      cur = {};
      cur.tick = std::atoi(line.c_str() + 5);
    }
    else if (line.rfind("v ", 0) == 0)
    {
      std::istringstream ss(line.substr(2));
      double x;
      while (ss >> x) cur.v.push_back(x);
    }
  }
  if (cur.tick >= 0) out.push_back(cur);
  return !out.empty();
}

}  // namespace

int main(int argc, char **argv)
{
  const std::string goldenDir = argc > 1 ? argv[1] : "build-golden/force";
  constexpr double kEps = 1e-9;

  // Per-scenario results kept for the cross-scenario criteria below.
  double filterFreeSigma = 0, filterFreeCV = 0;
  double filterSyncRatio = 0, filterSyncCollapse = 0, filterSplayCV = 0;
  double phaserSyncRatio = 0, phaserSplayCV = 0;
  double echoFreeSigma = 0, echoSyncRatio = 0, echoSplayCV = 0;

  for (const Scenario &sc : kScenarios)
  {
    Setup u = build(sc);
    std::vector<Checkpoint> golden;
    if (!loadGolden(goldenDir + "/" + std::string(sc.name) + ".golden", golden))
    {
      std::printf("FAIL %s: golden missing (run gen_force_goldens.mjs)\n", sc.name);
      g_failures++;
      continue;
    }
    const GravStat g0 = gravStat(u.pop, u.att, sc.basin);
    const double sigma0 = sigmaOf(u.pop.v, u.pop.n);

    size_t gi = 0;
    double worst = 0;
    auto compare = [&](int t) {
      if (gi < golden.size() && golden[gi].tick == t)
      {
        for (int i = 0; i < u.pop.n && i < (int)golden[gi].v.size(); i++)
          worst = std::max(worst, std::fabs(u.pop.v[i] - golden[gi].v[i]));
        gi++;
      }
    };
    compare(0);
    for (int t = 1; t <= kTicks; t++)
    {
      tick(u.pop, u.params, u.prof, u.att, kDt);
      compare(t);
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s: trajectory matches JS lab (worst |dv|=%%g)", sc.name);
    check(gi == golden.size() && worst <= kEps, buf, worst);

    /* criteria (population halves of L0-14/15/17/19/20) */
    const std::string n = sc.name;
    const double sigmaF = sigmaOf(u.pop.v, u.pop.n);
    if (n == "filter-free")
    {
      filterFreeSigma = sigmaF;
      filterFreeCV = gapCVOf(u.pop.v, u.pop.n);
      check(std::fabs(filterFreeSigma - 0.755) <= 0.01,
            "L0-14 anchor: free sigma 0.755 got=%g", filterFreeSigma);
      check(std::fabs(filterFreeCV - 0.123) <= 0.005, "L0-14 anchor: free CV 0.123 got=%g",
            filterFreeCV);
    }
    else if (n == "filter-sync")
    {
      filterSyncRatio = sigmaF / sigma0;
      filterSyncCollapse = u.pop.collapse;
      check(filterSyncRatio >= 0.15 && filterSyncRatio <= 0.25,
            "L0-14 K=+1: sigma ratio 0.20+-0.05 got=%g", filterSyncRatio);
      check(filterSyncCollapse >= 0.75, "L0-14 K=+1: collapse >= 0.75 got=%g",
            filterSyncCollapse);
    }
    else if (n == "filter-splay")
    {
      filterSplayCV = gapCVOf(u.pop.v, u.pop.n);
      check(filterSplayCV <= 0.02, "L0-14 K=-1: gap CV <= 0.02 got=%g", filterSplayCV);
    }
    else if (n == "filter-grav")
    {
      const GravStat gf = gravStat(u.pop, u.att, sc.basin);
      const double law = 1.5 / (1.5 + 6 * 0.8), ratio = gf.mean / g0.mean;
      check(g0.cnt >= 3 && std::fabs(ratio - law) <= 0.2 * law,
            "L0-15 gravity: equilibrium ratio h/(h+g)+-20%% got=%g", ratio);
    }
    else if (n == "filter-antigrav")
    {
      const GravStat gf = gravStat(u.pop, u.att, sc.basin);
      check(g0.within40 >= 3 && gf.within40 == 0,
            "L0-15 anti-gravity: within-40c count 0 (from >=3) got=%g",
            (double)gf.within40);
    }
    else if (n == "phaser-sync")
    {
      phaserSyncRatio = sigmaF / sigma0;
      check(phaserSyncRatio >= 0.15 && phaserSyncRatio <= 0.25,
            "L0-17 shared-core: phaser sigma ratio 0.20+-0.05 got=%g", phaserSyncRatio);
      check(std::fabs(phaserSyncRatio - filterSyncRatio) <= 0.02,
            "L0-17 shared-core: phaser ratio matches bank got=%g",
            std::fabs(phaserSyncRatio - filterSyncRatio));
    }
    else if (n == "phaser-splay")
    {
      phaserSplayCV = gapCVOf(u.pop.v, u.pop.n);
      check(std::fabs(phaserSplayCV - filterSplayCV) <= 0.01,
            "L0-17 shared-core: phaser gap CV matches bank got=%g",
            std::fabs(phaserSplayCV - filterSplayCV));
    }
    else if (n == "echo-free")
    {
      echoFreeSigma = sigmaF;
    }
    else if (n == "echo-sync")
    {
      echoSyncRatio = sigmaF / echoFreeSigma;
      check(echoSyncRatio >= 0.15 && echoSyncRatio <= 0.25,
            "L0-19 gather: sigma ratio 0.20+-0.05 got=%g", echoSyncRatio);
    }
    else if (n == "echo-splay")
    {
      echoSplayCV = gapCVOf(u.pop.v, u.pop.n);
      check(echoSplayCV <= 0.08, "L0-19 grid: gap CV <= 0.08 got=%g", echoSplayCV);
    }
    else if (n == "echo-grav")
    {
      const GravStat gf = gravStat(u.pop, u.att, sc.basin);
      const double law = 1.5 / (1.5 + 6 * 0.8), ratio = gf.mean / g0.mean;
      // >=1 captured member, not >=2: rhythmic attractors are sparse relative
      // to the tap spread and the reference scenario itself captures exactly
      // one (probed 2026-07-18). The criterion is the ratio, per ACCEPTANCE.
      check(g0.cnt >= 1 && std::fabs(ratio - law) <= 0.2 * law,
            "L0-19 rhythmic gravity: equilibrium ratio +-20%% got=%g", ratio);
    }
    else if (n == "room-symp")
    {
      const GravStat gf = gravStat(u.pop, u.att, sc.basin);
      const double law = 1.5 / (1.5 + 6 * 0.9), ratio = gf.mean / g0.mean;
      check(g0.cnt >= 2 && std::fabs(ratio - law) <= 0.2 * law,
            "L0-20 sympathetic: equilibrium ratio +-20%% got=%g", ratio);
    }
  }

  std::printf("force_check: %s (%d failure%s)\n", g_failures ? "RED" : "GREEN", g_failures,
              g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
