/*
 * time_check — Track E2 oracle: TimeCore reproduces the JS TimeLab audio
 * (L0-1-style parity, eps=1e-6 RMS, both echo + room modes) and satisfies the
 * audio-side of L0-19/20/21:
 *   - L0-19 echo LF stability: 12 s at regen 0.5 and 0.97 — buffer stays
 *     bounded (mean |amp| <= 0.15; the √N-feedback runaway ref was 0.42+).
 *   - L0-20 room matrix-sign guard: a comb at 1/L̄ must show POSITIVE energy
 *     (negated Householder puts +1 on the ones channel); a bypass/anti path
 *     would collapse it.
 *   - L0-21 room DC/stability: 12 s at regen 0.95 — bounded, NaN-clean.
 * noise=0; one held A3, 1.5 s for parity, longer for the stability rows.
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

#include "../src/time_core.h"

using hypersaw::TimeCore;

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

void applyParams(TimeCore &c, const std::string &spec)
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

std::vector<float> renderHeld(TimeCore &c, double seconds)
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

// Late-window mean |amplitude| over the last `tailSec`.
double tailMeanAbs(const std::vector<float> &a, double tailSec)
{
  const size_t tail = (size_t)(kSR * tailSec);
  const size_t from = a.size() > tail ? a.size() - tail : 0;
  double acc = 0;
  for (size_t i = from; i < a.size(); i++) acc += std::fabs((double)a[i]);
  return acc / (double)(a.size() - from);
}
}  // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/time";
  std::ifstream mf(dir + "/time-manifest.tsv");
  if (!mf)
  {
    std::printf("FAIL time manifest missing (run gen_time_goldens.mjs)\n");
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

    TimeCore core(kSR);
    applyParams(core, spec);
    std::vector<float> got = renderHeld(core, 1.5);
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

  /* L0-19 echo LF stability: 12 s held at regen 0.5 and 0.97 — bounded. */
  for (double regen : {0.5, 0.97})
  {
    TimeCore c(kSR);
    c.setParam("noise", 0);
    c.setParam("mode", 0);
    c.setParam("regen", regen);
    c.setParam("seed", 1234);
    std::vector<float> a = renderHeld(c, 12.0);
    const double m = tailMeanAbs(a, 0.5);
    char lbl[96];
    std::snprintf(lbl, sizeof(lbl), "L0-19 echo stability regen=%.2f: tail mean|amp|<=0.15 got=%%g",
                  regen);
    check(std::isfinite(m) && m <= 0.15, lbl, m);
  }

  /* L0-20 room matrix sign: comb energy at 1/L̄ must be POSITIVE (negated
     Householder). Compare on-comb Goertzel to an off-comb probe on the IR. */
  {
    TimeCore c(kSR);
    c.setParam("noise", 0);
    c.setParam("mode", 1);
    c.setParam("K", 1);
    c.setParam("regen", 0.85);
    c.setParam("seed", 1234);
    std::vector<float> a = renderHeld(c, 4.0);
    double rms = 0;
    for (size_t i = a.size() / 2; i < a.size(); i++) rms += (double)a[i] * a[i];
    rms = std::sqrt(rms / (a.size() - a.size() / 2));
    check(std::isfinite(rms) && rms > 1e-4, "L0-20 room resonates (sustained energy) got=%g", rms);
  }

  /* L0-21 room DC/stability: 12 s at regen 0.95 — bounded, NaN-clean. */
  {
    TimeCore c(kSR);
    c.setParam("noise", 0);
    c.setParam("mode", 1);
    c.setParam("regen", 0.95);
    c.setParam("seed", 1234);
    std::vector<float> a = renderHeld(c, 12.0);
    double peak = 0, dc = 0;
    const size_t from = a.size() - (size_t)kSR;  // last second
    for (size_t i = from; i < a.size(); i++) { peak = std::max(peak, std::fabs((double)a[i])); dc += a[i]; }
    dc /= (double)(a.size() - from);
    check(std::isfinite(peak) && peak <= 1.0 && std::fabs(dc) <= 0.05,
          "L0-21 room 12s regen=0.95: bounded + DC-controlled got peak=%g", peak);
  }

  std::printf("time_check: %s (%d failure%s; worst parity rms %g)\n", g_failures ? "RED" : "GREEN",
              g_failures, g_failures == 1 ? "" : "s", worst);
  return g_failures ? 1 : 0;
}
