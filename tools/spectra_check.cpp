/*
 * spectra_check — Phase 4 oracle for the SPECTRA engine.
 *  1. PARITY: SpectraCore vs the JS reference renders (spectra goldens),
 *     RMS(diff) <= 1e-6 per scenario — the L0-1 contract applied to the
 *     third reference.
 *  2. L0-6 cascade zipper: K=+0.9, cascade=1, retrig off, P=12 — the lock
 *     front (all R_0..k-1 > 0.85) advances monotonically and completes
 *     within [4.9, 13] s (7-10 s reference +-30%); cascade=0.5 completes
 *     <= 3 s. Front monotonicity is the blocking half.
 *  3. L0-7 interference gate: K=-1 vs K=0 at defaults, 4 s settle — RMS
 *     reduction >= 12 dB (reference -14.8; a value near -1.7 dB means the
 *     wmix narrowing path broke).
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

#include "../src/spectra_core.h"
#include "../src/swarm_core.h"

using hypersaw::SpectraCore;

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

void applyParams(SpectraCore &c, const std::string &spec)
{
  if (spec == "-") return;
  std::istringstream ss(spec);
  std::string kv;
  while (ss >> kv)
  {
    const auto eq = kv.find('=');
    c.setParam(kv.substr(0, eq).c_str(), std::atof(kv.c_str() + eq + 1));
  }
}

// Render `seconds` with a held note started at t=0; returns interleaved f32.
std::vector<float> renderHeld(SpectraCore &c, double seconds)
{
  c.noteOn(kNoteM, kNoteF);
  const int total = (int)(kSR * seconds);
  std::vector<float> out((size_t)total * 2);
  float L[kBlock], R[kBlock];
  for (int off = 0; off < total; off += kBlock)
  {
    c.render(L, R, kBlock);
    for (int i = 0; i < kBlock && off + i < total; i++)
    {
      out[(size_t)(off + i) * 2] = L[i];
      out[(size_t)(off + i) * 2 + 1] = R[i];
    }
  }
  return out;
}

double rmsTail(const std::vector<float> &a, double tailSeconds)
{
  const size_t tail = (size_t)(kSR * tailSeconds) * 2;
  const size_t from = a.size() > tail ? a.size() - tail : 0;
  double acc = 0;
  for (size_t i = from; i < a.size(); i++) acc += (double)a[i] * a[i];
  return std::sqrt(acc / (double)(a.size() - from));
}
}  // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/spectra";

  /* ---- 1. parity vs the JS reference ---- */
  std::ifstream mf(dir + "/spectra-manifest.tsv");
  if (!mf)
  {
    std::printf("FAIL spectra manifest missing (run gen_spectra_goldens.mjs)\n");
    return 1;
  }
  std::string line;
  double worstAll = 0;
  while (std::getline(mf, line))
  {
    if (line.empty()) continue;
    const auto tab = line.find('\t');
    const std::string name = line.substr(0, tab), spec = line.substr(tab + 1);
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

    SpectraCore core(kSR);
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
    worstAll = std::max(worstAll, rms);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "parity %s (rms=%%g, ref rms %.3f)", name.c_str(), refRms);
    check(ref.size() == got.size() && rms <= 1e-6 && refRms > 1e-4, buf, rms);
  }

  /* ---- 2. L0-6 cascade zipper ---- */
  for (const double casc : {1.0, 0.5})
  {
    SpectraCore core(kSR);
    core.setParam("K", 0.9);
    core.setParam("cascade", casc);
    core.setParam("retrig", 0);
    core.setParam("partials", 12);
    core.noteOn(kNoteM, kNoteF);
    float L[kBlock], R[kBlock];
    int front = 0, maxFront = 0;
    bool monotone = true;
    double doneAt = -1;
    const double horizon = casc == 1.0 ? 14.0 : 4.0;
    const int totalBlocks = (int)(kSR * horizon) / kBlock;
    for (int b = 0; b < totalBlocks; b++)
    {
      core.render(L, R, kBlock);
      const auto *s = core.focus();
      front = 0;
      while (front < 12 && s->R[front] > 0.85) front++;
      // the front may flicker one partial at the boundary; a drop > 1 from
      // the high-water mark means the zipper genuinely retreated
      if (front + 1 < maxFront) monotone = false;
      maxFront = std::max(maxFront, front);
      if (front == 12 && doneAt < 0) doneAt = (double)(b + 1) * kBlock / kSR;
    }
    if (casc == 1.0)
    {
      check(monotone, "L0-6 cascade=1: lock front monotone (violations=%g)", monotone ? 0 : 1);
      check(doneAt > 4.9 && doneAt < 13.0, "L0-6 cascade=1: completes in 7-10s +-30%% got=%g s",
            doneAt);
    }
    else
    {
      check(doneAt > 0 && doneAt <= 3.0, "L0-6 cascade=0.5: completes <= 3 s got=%g s", doneAt);
    }
  }

  /* ---- 3. L0-7 interference gate ---- */
  {
    SpectraCore base(kSR);
    std::vector<float> a = renderHeld(base, 4.5);
    SpectraCore splay(kSR);
    splay.setParam("K", -1);
    std::vector<float> b = renderHeld(splay, 4.5);
    const double dB = 20 * std::log10(rmsTail(b, 0.5) / rmsTail(a, 0.5));
    check(dB <= -12.0, "L0-7 interference gate: >= 12 dB reduction got=%g dB", dB);
    // narrowing must be engaged (wmix -> 1 under full splay)
    double wm = 0;
    const auto *s = splay.focus();
    for (int k = 0; k < 12; k++) wm = std::max(wm, s->wmix[k]);
    check(wm > 0.5, "L0-7 narrowing active: max wmix got=%g", wm);
  }

  /* ---- 4. Phase 4 gate (ADR-037 ruling a): SAW = SPECTRA at P=1, measured.
     Equivalent settings: SAW n=5/even/cents/detune 0.1 vs SPECTRA P=1/
     cloud 5/cwidth 0.25/wlaw 0 — both put voices at x*10 cents around A3.
     At P=1 the references' dynamics expressions coincide (coupling, slews,
     Kenv, rng stream, initial phases); the kernel is the only difference,
     so R must track tick-for-tick. Everything inert stays at defaults on
     the SAW side (drift/gravity/glide/tune off; lpOut 0). ---- */
  {
    hypersaw::SwarmCore saw(kSR);
    saw.setParam("n", 5);
    saw.setParam("dist", 0);
    saw.setParam("law", 0);
    saw.setParam("detune", 0.1);
    saw.setParam("lpOut", 0);
    saw.setParam("retrig", 0);
    saw.setParam("K", 0.8);
    SpectraCore spc(kSR);
    spc.setParam("partials", 1);
    spc.setParam("cloud", 5);
    spc.setParam("cwidth", 0.25);
    spc.setParam("wlaw", 0);
    spc.setParam("retrig", 0);
    spc.setParam("K", 0.8);
    const int sawSlot = saw.noteOn(kNoteM, kNoteF);
    const int spcSlot = spc.noteOn(kNoteM, kNoteF);
    float L[kBlock], R2[kBlock];
    double worstR = 0;
    for (int b = 0; b < (int)(kSR * 4) / kBlock; b++)
    {
      saw.render(L, R2, kBlock);
      spc.render(L, R2, kBlock);
      worstR = std::max(worstR,
                        std::fabs(saw.voiceAt(sawSlot).R - spc.voiceAt(spcSlot).R[0]));
    }
    check(worstR <= 1e-9, "P=1 gate (ADR-037a): SAW vs SPECTRA R tick-locked, worst dR=%g",
          worstR);
  }

  std::printf("spectra_check: %s (%d failure%s; worst parity rms %g)\n",
              g_failures ? "RED" : "GREEN", g_failures, g_failures == 1 ? "" : "s", worstAll);
  return g_failures ? 1 : 0;
}
