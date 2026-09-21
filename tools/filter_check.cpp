/*
 * filter_check — Track E1 oracle: FilterCore (resonator bank) reproduces the
 * JS FilterLab audio (L0-1-style parity, eps=1e-6 RMS) and satisfies the
 * audio halves of ACCEPTANCE L0-14 (collapse→Q). The population halves of
 * L0-14/15 are already covered by force_check; this pins the AUDIO path
 * (exciter + SVF bank + collapse→Q + wet/dry).
 *
 * All scenarios run at noise=0 (the reference's dither is Math.random(), not
 * bit-checkable — see filter_core.h). One held A3, 1 s, 512-sample blocks.
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

#include "../src/filter_core.h"

using hypersaw::FilterCore;

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

void applyParams(FilterCore &c, const std::string &spec)
{
  std::istringstream ss(spec);
  std::string kv;
  std::string seed;
  while (ss >> kv)
  {
    const auto eq = kv.find('=');
    const std::string key = kv.substr(0, eq), val = kv.substr(eq + 1);
    if (key == "seed") { seed = val; continue; }  // apply last (mirror the generator)
    c.setParam(key.c_str(), std::atof(val.c_str()));
  }
  if (!seed.empty()) c.setParam("seed", std::atof(seed.c_str()));
}

std::vector<float> renderHeld(FilterCore &c, double seconds)
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
}  // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/filter";
  std::ifstream mf(dir + "/filter-manifest.tsv");
  if (!mf)
  {
    std::printf("FAIL filter manifest missing (run gen_filter_goldens.mjs)\n");
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

    FilterCore core(kSR);
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

  /* L0-14 audio half: collapse→Q lifts resonance. K=+1 must raise the effective
     Q well above the base (collapse widens it) and produce audible ring. */
  {
    FilterCore base(kSR);
    base.setParam("noise", 0);
    renderHeld(base, 2.0);
    const double qFree = base.qEffective();
    FilterCore sync(kSR);
    sync.setParam("noise", 0);
    sync.setParam("K", 1);
    sync.setParam("q2col", 1);
    renderHeld(sync, 2.0);
    check(sync.qEffective() > qFree * 1.5 && sync.collapse() >= 0.75,
          "L0-14 collapse->Q: sync raises effective Q got=%g", sync.qEffective());
  }

  std::printf("filter_check: %s (%d failure%s; worst parity rms %g)\n", g_failures ? "RED" : "GREEN",
              g_failures, g_failures == 1 ? "" : "s", worst);
  return g_failures ? 1 : 0;
}
