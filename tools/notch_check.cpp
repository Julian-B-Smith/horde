/*
 * notch_check — Track E1.2 oracle: NotchCore reproduces the JS PhaserLab audio
 * (L0-1-style parity, eps=1e-6 RMS) and satisfies the audio half of L0-16
 * (notch exactness — the regression guard against the allpass-topology defect,
 * ADR-029b: a true-notch cascade nulls DEEPLY at its centers; scattered/shallow
 * attenuation means the defect returned).
 *
 * noise=0 for every scenario. One held A3, 1 s, 512-sample blocks.
 * WIRED: ./verify full.
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

#include "../src/notch_core.h"

using hypersaw::NotchCore;

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

void applyParams(NotchCore &c, const std::string &spec)
{
  std::istringstream ss(spec);
  std::string kv, seed;
  while (ss >> kv)
  {
    const auto eq = kv.find('=');
    const std::string key = kv.substr(0, eq), val = kv.substr(eq + 1);
    if (key == "seed") { seed = val; continue; }
    c.setParam(key.c_str(), std::atof(val.c_str()));
  }
  if (!seed.empty()) c.setParam("seed", std::atof(seed.c_str()));
}

std::vector<float> renderHeld(NotchCore &c, double seconds)
{
  c.noteOn(kNoteM, kNoteF);
  const int total = (int)(kSR * seconds);
  std::vector<float> out(total);
  float L[kBlock], R[kBlock];
  for (int off = 0; off < total; off += kBlock)
  {
    c.render(L, R, kBlock);
    for (int i = 0; i < kBlock && off + i < total; i++) out[off + i] = L[i];
  }
  return out;
}

// Drive chain() with a steady sine at `f` (feedback=0), measure output/input
// RMS ratio in dB after a settle. Exercises the notch response directly.
double chainAttenDb(NotchCore &c, double f)
{
  constexpr double kPi = 3.141592653589793;  // no M_PI on MSVC (L0003)
  double inAcc = 0, outAcc = 0;
  const int n = (int)(kSR * 0.25);
  for (int i = 0; i < n; i++)
  {
    const double x = std::sin(2 * kPi * f * i / kSR);
    const double y = c.chain(x);
    if (i > n / 2)  // measure the settled tail
    {
      inAcc += x * x;
      outAcc += y * y;
    }
  }
  return 10 * std::log10(outAcc / inAcc);
}
}  // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/notch";
  std::ifstream mf(dir + "/notch-manifest.tsv");
  if (!mf)
  {
    std::printf("FAIL notch manifest missing (run gen_notch_goldens.mjs)\n");
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

    NotchCore core(kSR);
    applyParams(core, spec);
    std::vector<float> got = renderHeld(core, 1.0);
    double acc = 0, era = 0;
    const size_t n = std::min(ref.size(), got.size());
    for (size_t i = 0; i < n; i++)
    {
      const double d = (double)got[i] - ref[i];
      acc += d * d;
      era += (double)ref[i] * ref[i];
    }
    const double rms = std::sqrt(acc / n), refRms = std::sqrt(era / n);
    worst = std::max(worst, rms);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "parity %s (rms=%%g, ref rms %.3f)", name.c_str(), refRms);
    check(ref.size() == got.size() && rms <= 1e-6 && refRms > 1e-4, buf, rms);
  }

  /* L0-16 audio half: true-notch exactness. feedback=0, K=0; the cascade must
     null DEEPLY at a swarm frequency vs an off-notch probe a quarter-octave
     away. The allpass defect (ADR-029b) would scatter these to within ~10 dB. */
  {
    NotchCore c(kSR);
    c.setParam("noise", 0);
    c.setParam("feedback", 0);
    c.setParam("K", 0);
    c.setParam("nb", 6);
    c.setParam("seed", 1234);
    c.controlTick();  // set coefficients at the home layout
    const double fc = c.bandFreq(0);           // a swarm notch center
    const double off = fc * std::pow(2, 0.25);  // a quarter-octave away
    NotchCore a(kSR), b(kSR);
    for (auto *n : {&a, &b}) { n->setParam("noise", 0); n->setParam("feedback", 0); n->setParam("K", 0); n->setParam("nb", 6); n->setParam("seed", 1234); n->controlTick(); }
    const double onNotch = chainAttenDb(a, fc);
    const double offNotch = chainAttenDb(b, off);
    check(offNotch - onNotch >= 20.0,
          "L0-16 notch exactness: on-notch >= 20dB deeper than off got=%g dB deeper",
          offNotch - onNotch);
  }

  std::printf("notch_check: %s (%d failure%s; worst parity rms %g)\n", g_failures ? "RED" : "GREEN",
              g_failures, g_failures == 1 ? "" : "s", worst);
  return g_failures ? 1 : 0;
}
