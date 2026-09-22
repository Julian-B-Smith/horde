/* stability_check — the engine over TEN MINUTES of held notes.
 * WIRED: ./verify full.
 *
 * B147 layer 2. The B147 layer-1 audit (docs/audits/2026-09-18-saw-engine-audit.md
 * §3.4) found: "Gate: none for duration. `notefuzz_check` and
 * `robustness_matrix` cover finiteness and tails at seconds-scale;
 * ACCEPTANCE L0-13 asserts stability without a duration." Nothing in
 * `./verify` runs longer than seconds, and a coupled-oscillator system with a
 * momentum integrator, a random walk, a nonlinear pull and a feedback allpass
 * is exactly the shape whose failures live at minute scale, not second scale.
 *
 * Nine patches — the audit's own list, reproduced here so the numbers are
 * comparable row for row — three held notes, 128-frame blocks, 44.1 kHz,
 * 10 minutes of SIMULATED time each. Five quantities per patch:
 *
 *   RMS drift   |RMS(last 10 s) - RMS(first 10 s)| in dB
 *   f0 drift    the lowest gated voice's f0cur at t=10 s vs at the end, cents
 *   peak        max |output| over the whole run
 *   max |Ksm|   the largest coupling-smoother magnitude seen
 *   non-finite  count of NaN/Inf output samples
 *
 * THRESHOLDS — the audit's measured worst column, plus a stated margin:
 *
 *   quantity      audit worst   bar      margin and why
 *   RMS drift     0.41 dB       1.0 dB   ~2.4x. The engine is bounded by the
 *                                        output tanh, so the honest failure is
 *                                        a SLOW walk; 1 dB over ten minutes is
 *                                        still inaudible and is 0.6 dB clear
 *                                        of today's worst.
 *   f0 drift      0.001 ct      0.01 ct  10x. Today's number is at the probe's
 *                                        own resolution (a 0.001 ct reading is
 *                                        one part in 5.8e-7 of the frequency),
 *                                        so a tighter bar would be gating
 *                                        double-precision noise, not the
 *                                        engine.
 *   peak          0.7821        1.00     the tanh's own bound. Not a margin: a
 *                                        peak above 1.0 is a clipped output,
 *                                        which is a different claim from
 *                                        "bounded", and 1.0 is where the claim
 *                                        actually lives.
 *   max |Ksm|     50.848        200      ~3.9x. |Ksm| is unbounded by
 *                                        construction (4K^2*sigma, and sigma
 *                                        grows with detune and spread), so the
 *                                        bar is a DIVERGENCE bar, not a
 *                                        tightness bar; 200 is well below
 *                                        anything a runaway would sit at and
 *                                        well above every legal patch here.
 *   non-finite    0             0        exact. There is no margin on a NaN.
 *
 * CONTROLS (L0032 — every one of these metrics is a "reads zero" metric, and a
 * metric that has never been shown reading non-zero has never been shown able
 * to fail): the default patch's own render is re-scored three times with a
 * plant applied to the SAMPLES — a slow divergent gain (must fail the RMS
 * bar), a single planted NaN (must fail the finiteness bar), and a planted
 * over-unity sample (must fail the peak bar). The plants act on the measured
 * buffer, not on the engine: they test the scorer, which is the thing whose
 * zeros are being trusted.
 *
 * RUNTIME. Measured on the B147 machine: see the footer this prints. If that
 * is more than you want to wait, `--seconds=N` shortens every patch; the
 * DEFAULT IS THE FULL 600 s, because a duration check whose default duration
 * is short is a duration check in name only.
 *
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../src/swarm_core.h"

namespace
{

int g_failures = 0;
void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "PASS" : "FAIL", what, detail);
  if (!ok) g_failures++;
}

constexpr double kSr = 44100.0;
constexpr int kBlk = 128;

struct Score
{
  double rmsFirstDb, rmsLastDb, peak, f0Cents, maxKsm, effLo, effHi;
  long nonFinite;
};

// What the nine patches are scored ON. Kept separate from the render so the
// controls below can re-score a PLANTED copy of the same numbers — the plants
// have to reach the scorer, not the engine, or they would be testing something
// else.
struct Accum
{
  double sumFirst = 0, sumLast = 0, peak = 0, maxKsm = 0;
  long nFirst = 0, nLast = 0, nonFinite = 0;
  void sample(double v, bool inFirst, bool inLast)
  {
    if (!std::isfinite(v)) { nonFinite++; return; }
    const double a = std::fabs(v);
    if (a > peak) peak = a;
    if (inFirst) { sumFirst += v * v; nFirst++; }
    if (inLast) { sumLast += v * v; nLast++; }
  }
  double firstDb() const { return 10 * std::log10(std::fmax(sumFirst / std::fmax(1.0, (double)nFirst), 1e-300)); }
  double lastDb() const { return 10 * std::log10(std::fmax(sumLast / std::fmax(1.0, (double)nLast), 1e-300)); }
};

struct Patch
{
  const char *name;
  // key/value pairs applied through setParam, so the patch reads as the human
  // would describe it and a wrong key is a compile-visible typo, not a silent
  // default.
  std::vector<std::pair<const char *, double>> kv;
};

Score runPatch(const Patch &pt, double seconds, std::vector<float> *keepSamples)
{
  hypersaw::SwarmCore c(kSr);
  c.setParam("seed", 1234);
  for (const auto &x : pt.kv)
    if (!c.setParam(x.first, x.second)) { std::printf("FAIL  unknown param key '%s'\n", x.first); g_failures++; }
  const int s0 = c.noteOn(60, 261.6255653005986);
  c.noteOn(64, 329.6275569128699);
  c.noteOn(67, 391.99543598174927);

  const long total = (long)(seconds * kSr);
  const long firstEnd = (long)(10.0 * kSr);
  const long lastStart = total - (long)(10.0 * kSr);
  const long baseAt = firstEnd;   // f0 baseline at t = 10 s, after any settle

  Accum a;
  std::vector<float> L(kBlk), R(kBlk);
  double f0Base = 0, f0End = 0, effLo = 1e30, effHi = -1e30;
  long done = 0;
  while (done < total)
  {
    const int m = (int)std::min<long>(kBlk, total - done);
    c.render(L.data(), R.data(), m);
    for (int i = 0; i < m; i++)
    {
      const double v = L[i];
      a.sample(v, done + i < firstEnd, done + i >= lastStart);
      if (keepSamples) keepSamples->push_back(L[i]);
    }
    const auto &s = c.voiceAt(s0);
    a.maxKsm = std::fmax(a.maxKsm, std::fmax(std::fabs(s.KsmS), std::fmax(std::fabs(s.KsmP), std::fabs(s.KsmD))));
    if (done <= baseAt && done + m > baseAt) f0Base = s.f0cur;
    done += m;
  }
  {
    const auto &s = c.voiceAt(s0);
    f0End = s.f0cur;
    const int n = (int)c.getParam("n");
    for (int i = 0; i < n && i < hypersaw::kMaxV; i++)
    { effLo = std::fmin(effLo, s.eff[i]); effHi = std::fmax(effHi, s.eff[i]); }
  }
  const double cents = (f0Base > 0 && f0End > 0) ? std::fabs(1200.0 * std::log2(f0End / f0Base)) : -1;
  return {a.firstDb(), a.lastDb(), a.peak, cents, a.maxKsm, effLo, effHi, a.nonFinite};
}

// Re-score an already-rendered buffer. The controls' only job.
Accum rescore(const std::vector<float> &x, long total)
{
  Accum a;
  const long firstEnd = (long)(10.0 * kSr), lastStart = total - (long)(10.0 * kSr);
  for (long i = 0; i < (long)x.size(); i++) a.sample(x[i], i < firstEnd, i >= lastStart);
  return a;
}

}  // namespace

int main(int argc, char **argv)
{
  double seconds = 600.0;
  for (int i = 1; i < argc; i++)
    if (std::strncmp(argv[i], "--seconds=", 10) == 0) seconds = std::atof(argv[i] + 10);
  if (seconds < 25.0) { std::printf("--seconds must be >= 25 (two 10 s windows plus settle)\n"); return 2; }

  std::printf("stability_check — %.0f minutes of simulated time per patch, %d patches\n",
              seconds / 60, 9);
  std::printf("(B147 layer 2; thresholds are the 2026-09-18 audit's measured worst + a stated margin)\n\n");

  const std::vector<Patch> patches = {
      {"default (K 0)", {}},
      {"K 1 locked", {{"K", 1}}},
      {"K -1 splay", {{"K", -1}}},
      {"drift 1 walk", {{"driftDepth", 100}, {"driftRate", 0.4}, {"driftMode", 0}}},
      {"gravity 1 basin 60", {{"grav", 1}, {"basin", 60}}},
      {"inertia 0.95 + K1", {{"inertia", 0.95}, {"K", 1}}},
      {"ring topo r5 K1", {{"topo", 1}, {"reach", 5}, {"K", 1}}},
      {"two-cluster bal 1 K-1", {{"topo", 2}, {"balance", 1}, {"K", -1}}},
      {"everything (n16 OS drift grav motion glide)",
       {{"n", 16}, {"oversample", 1}, {"driftDepth", 100}, {"grav", 0.7}, {"basin", 50},
        {"panMotion", 0.6}, {"glide", 0.3}, {"K", 0.6}, {"inertia", 0.5}}},
  };

  const auto t0 = std::chrono::steady_clock::now();
  std::printf("%-44s %9s %9s %8s %9s %9s %6s\n",
              "patch", "RMS0(dB)", "RMSn(dB)", "peak", "f0 (ct)", "max|Ksm|", "nonfin");
  std::vector<float> keep;   // the default patch only, for the controls
  std::vector<Score> scores;
  for (size_t i = 0; i < patches.size(); i++)
  {
    const Score s = runPatch(patches[i], seconds, i == 0 ? &keep : nullptr);
    scores.push_back(s);
    std::printf("%-44s %9.2f %9.2f %8.4f %9.3f %9.3f %6ld\n",
                patches[i].name, s.rmsFirstDb, s.rmsLastDb, s.peak, s.f0Cents, s.maxKsm, s.nonFinite);
  }
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

  std::printf("\n-- controls (the SCORER, planted on the default patch's own samples) --\n");
  {
    const long total = (long)(seconds * kSr);
    char d[220];
    const Accum clean = rescore(keep, total);
    std::snprintf(d, sizeof(d), "unplanted rescore: RMS drift %.4f dB, peak %.4f, non-finite %ld",
                  std::fabs(clean.lastDb() - clean.firstDb()), clean.peak, clean.nonFinite);
    check(std::fabs(clean.lastDb() - clean.firstDb()) < 1.0 && clean.nonFinite == 0,
          "CONTROL must-read-clean: the scorer agrees with the render", d);

    // A slow divergent gain: +3 dB linearly over the run. This is the failure
    // shape the RMS column exists for, and it must be caught.
    std::vector<float> planted = keep;
    for (long i = 0; i < (long)planted.size(); i++)
      planted[i] = (float)(planted[i] * std::pow(10.0, 3.0 * ((double)i / planted.size()) / 20.0));
    const Accum gain = rescore(planted, total);
    std::snprintf(d, sizeof(d), "+3 dB ramp over the run reads %.4f dB of drift (bar is 1.0)",
                  std::fabs(gain.lastDb() - gain.firstDb()));
    check(std::fabs(gain.lastDb() - gain.firstDb()) > 1.0, "CONTROL must-fail: a planted divergent gain", d);

    std::vector<float> nan1 = keep;
    nan1[nan1.size() / 2] = std::nanf("");
    const Accum nn = rescore(nan1, total);
    std::snprintf(d, sizeof(d), "one planted NaN reads %ld non-finite samples", nn.nonFinite);
    check(nn.nonFinite == 1, "CONTROL must-fail: a planted NaN", d);

    std::vector<float> big = keep;
    big[big.size() / 3] = 1.5f;
    const Accum bg = rescore(big, total);
    std::snprintf(d, sizeof(d), "one planted 1.5 sample reads peak %.4f (bar is 1.0)", bg.peak);
    check(bg.peak > 1.0, "CONTROL must-fail: a planted over-unity peak", d);
  }

  std::printf("\n-- bars ----------------------------------------------------------\n");
  const double kBarRms = 1.0, kBarCents = 0.01, kBarPeak = 1.0, kBarKsm = 200.0;
  double wRms = 0, wCents = 0, wPeak = 0, wKsm = 0; long wNf = 0;
  for (const auto &s : scores)
  {
    wRms = std::fmax(wRms, std::fabs(s.rmsLastDb - s.rmsFirstDb));
    wCents = std::fmax(wCents, s.f0Cents);
    wPeak = std::fmax(wPeak, s.peak);
    wKsm = std::fmax(wKsm, s.maxKsm);
    wNf += s.nonFinite;
  }
  char d[220];
  std::snprintf(d, sizeof(d), "worst %.4f dB across 9 patches; audit 0.41, bar %.2f", wRms, kBarRms);
  check(wRms <= kBarRms, "RMS does not drift over ten minutes", d);
  std::snprintf(d, sizeof(d), "worst %.4f cents; audit 0.001, bar %.3f", wCents, kBarCents);
  check(wCents <= kBarCents, "pitch does not drift over ten minutes", d);
  std::snprintf(d, sizeof(d), "worst %.4f; audit 0.7821, bar %.2f (the tanh's bound)", wPeak, kBarPeak);
  check(wPeak <= kBarPeak, "the output stays bounded", d);
  std::snprintf(d, sizeof(d), "worst %.3f; audit 50.848, bar %.0f", wKsm, kBarKsm);
  check(wKsm <= kBarKsm, "the coupling smoother does not run away", d);
  std::snprintf(d, sizeof(d), "%ld non-finite samples across 9 patches", wNf);
  check(wNf == 0, "no NaN or Inf reaches the output", d);

  std::printf("\n%.1f s of wall time for %.0f s of simulated audio x 9 patches (--seconds=N to shorten; default 600).\n",
              wall, seconds);
  std::printf("stability_check: %s (%d failures)\n", g_failures ? "RED" : "GREEN", g_failures);
  return g_failures ? 1 : 0;
}
