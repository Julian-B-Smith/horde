/*
 * swarmalator_check — new-engine oracle: SwarmalatorCore reproduces the JS
 * Swarmalator STEREO audio (L0-1-style parity, eps=1e-6 RMS) and satisfies the
 * SPEC-SWARMALATOR §5 acceptance anchors:
 *   - K-sync == SAW when J=0: R >= 0.9 at K=+0.9, J=0.
 *   - splay: R low at K=-0.9, J=0.
 *   - rainbow: J alone raises max(R+,R-) >= 0.35 from a scattered start (R low).
 *   - sync+rainbow: K+J raise R and max(R+,R-) together.
 *   - stability at K=J=drift=1: bounded, NaN-clean.
 * Fully seed-deterministic; one held A3 (220 Hz), 1 s, 512-sample blocks.
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

#include "../src/swarmalator_core.h"

using hypersaw::SwarmalatorCore;

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
constexpr int kBlock = 512;
constexpr int kNoteM = 57;
const double kNoteF = 440 * std::pow(2, (kNoteM - 69) / 12.0);

void applyParams(SwarmalatorCore &c, const std::string &spec)
{
  std::istringstream ss(spec);
  std::string kv;
  while (ss >> kv)
  {
    const auto eq = kv.find('=');
    c.setParam(kv.substr(0, eq).c_str(), std::atof(kv.c_str() + eq + 1));
  }
}

// Render `seconds`, held A3; returns interleaved f32 and the settled order
// parameters (measured at the end).
struct RunResult
{
  std::vector<float> audio;
  double R, Rp, Rm, peak;
};
RunResult run(SwarmalatorCore &c, double seconds)
{
  c.noteOn(kNoteM, kNoteF);
  const int total = (int)(kSR * seconds);
  RunResult rr;
  rr.audio.resize((size_t)total * 2);
  rr.peak = 0;
  float L[kBlock], R[kBlock];
  for (int off = 0; off < total; off += kBlock)
  {
    c.render(L, R, kBlock);
    for (int i = 0; i < kBlock && off + i < total; i++)
    {
      rr.audio[(size_t)(off + i) * 2] = L[i];
      rr.audio[(size_t)(off + i) * 2 + 1] = R[i];
      rr.peak = std::max(rr.peak, std::max(std::fabs((double)L[i]), std::fabs((double)R[i])));
    }
  }
  rr.R = c.orderR();
  rr.Rp = c.orderRp();
  rr.Rm = c.orderRm();
  return rr;
}
}  // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/swarmalator";

  /* ---- 1. stereo parity vs the JS reference ---- */
  std::ifstream mf(dir + "/swarmalator-manifest.tsv");
  if (!mf)
  {
    std::printf("FAIL swarmalator manifest missing (run gen_swarmalator_goldens.mjs)\n");
    return 1;
  }
  std::string line;
  double worst = 0;
  while (std::getline(mf, line))
  {
    if (line.empty()) continue;
    const auto tab = line.find('\t');
    const std::string name = line.substr(0, tab);
    const std::string spec = tab == std::string::npos ? "" : line.substr(tab + 1);
    std::ifstream gf(dir + "/" + name + ".f32", std::ios::binary);
    if (!gf)
    {
      std::printf("FAIL %s: golden missing\n", name.c_str());
      g_failures++;
      continue;
    }
    std::vector<float> ref;
    gf.seekg(0, std::ios::end);
    ref.resize((size_t)gf.tellg() / 4);
    gf.seekg(0);
    gf.read((char *)ref.data(), (std::streamsize)ref.size() * 4);

    SwarmalatorCore core(kSR);
    applyParams(core, spec);
    RunResult got = run(core, 1.0);
    double acc = 0, era = 0;
    const size_t n = std::min(ref.size(), got.audio.size());
    for (size_t i = 0; i < n; i++)
    {
      const double d = (double)got.audio[i] - ref[i];
      acc += d * d;
      era += (double)ref[i] * ref[i];
    }
    const double rms = std::sqrt(acc / n), refRms = std::sqrt(era / n);
    worst = std::max(worst, rms);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "parity %s (rms=%%g, ref rms %.3f)", name.c_str(), refRms);
    check(ref.size() == got.audio.size() && rms <= 1e-6 && refRms > 1e-4, buf, rms);
  }

  /* ---- 2. SPEC §5 acceptance anchors ---- */
  auto measure = [](double K, double J, double drift) {
    SwarmalatorCore c(kSR);
    c.setParam("K", K);
    c.setParam("J", J);
    if (drift != 0.2) c.setParam("drift", drift);
    return run(c, 2.0);
  };
  const RunResult sync = measure(0.9, 0, 0.2);
  const RunResult splay = measure(-0.9, 0, 0.2);
  const RunResult rainbow = measure(0.0, 0.9, 0.2);
  const RunResult both = measure(0.6, 0.9, 0.2);
  const RunResult stab = measure(1, 1, 1);

  check(sync.R >= 0.9, "anchor sync: R>=0.9 at K=+0.9,J=0 (== SAW) got=%g", sync.R);
  check(splay.R < 0.3, "anchor splay: R low at K=-0.9,J=0 got=%g", splay.R);
  check(std::max(rainbow.Rp, rainbow.Rm) >= 0.35 && rainbow.R < 0.6,
        "anchor rainbow: max(R+,R-)>=0.35 with R incoherent got=%g",
        std::max(rainbow.Rp, rainbow.Rm));
  check(both.R > rainbow.R && std::max(both.Rp, both.Rm) > 0.6,
        "anchor sync+rainbow: R and max(R+,R-) both raised got=%g", std::max(both.Rp, both.Rm));
  check(std::isfinite(stab.peak) && stab.peak <= 1.0,
        "anchor stability: bounded + NaN-clean at K=J=drift=1 got peak=%g", stab.peak);

  std::printf("swarmalator_check: %s (%d failure%s; worst parity rms %g)\n",
              g_failures ? "RED" : "GREEN", g_failures, g_failures == 1 ? "" : "s", worst);
  return g_failures ? 1 : 0;
}
