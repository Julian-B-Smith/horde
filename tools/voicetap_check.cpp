/*
 * voicetap_check — the per-voice seam's proof (B81/B82 increment 1, ADR-148).
 *
 * The seam's whole contract is arithmetic: a tap that leaves its buffers
 * untouched must be BIT-IDENTICAL to no tap at all, across every render
 * feature that touches the note path (voice envelopes, oversampling+decimator,
 * super-width, onset scatter, glide) and across buffer subdivisions. Anything
 * less and "engaging the filter section" would change the sound before any
 * filter did — the exact defect the increment exists to make impossible.
 *
 *   T1  no-op tap == no tap, bit-exact, per config, odd chunk sizes
 *   T2  identity filter (one-pole with unit coefficient) == no tap, bit-exact
 *   T3  a REAL filter changes the output (the hook is genuinely in-path)
 *   T4  per-note buffers carry the right notes: gated slots non-silent,
 *       silent cores contribute nothing
 * WIRED: ./verify full.
 */
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>

#include "../src/swarm_core.h"

namespace
{
int failures = 0;
void check(bool ok, const char *what, const char *detail = "")
{
  std::printf("  %-58s %s%s%s\n", what, ok ? "ok" : "FAIL", *detail ? "  " : "", detail);
  if (!ok) failures++;
}

constexpr double kSR = 44100.0;
constexpr int kFrames = 2048;

struct Cfg { const char *name; std::vector<std::pair<std::string, double>> params; };

void drive(hypersaw::SwarmCore &c, const Cfg &cfg)
{
  for (auto &kv : cfg.params) c.setParam(kv.first, kv.second);
  c.noteOn(45, 110.0);
  c.noteOn(52, 164.81);
  c.noteOn(64, 329.63);
}

// render in deliberately odd chunks so subdivision is part of the proof
void renderAll(hypersaw::SwarmCore &c, float *L, float *R)
{
  int done = 0;
  const int chunks[] = {97, 256, 33, 500, 128};
  int ci = 0;
  while (done < kFrames)
  {
    int m = chunks[ci % 5]; ci++;
    if (m > kFrames - done) m = kFrames - done;
    c.render(L + done, R + done, m);
    done += m;
  }
}

void noopTap(void *, int, int, double *, double *, int) {}

struct IdFilter { double zL[16], zR[16]; };
void identityTap(void *ctx, int slot, int, double *l, double *r, int frames)
{
  // one-pole with UNIT coefficient: z += 1.0*(x - z) => z = x. Same values
  // through real filter code — the arithmetic identity T2 stands on.
  auto *f = (IdFilter *)ctx;
  for (int i = 0; i < frames; i++)
  {
    f->zL[slot] += 1.0 * (l[i] - f->zL[slot]); l[i] = f->zL[slot];
    f->zR[slot] += 1.0 * (r[i] - f->zR[slot]); r[i] = f->zR[slot];
  }
}
void lpTap(void *ctx, int slot, int, double *l, double *r, int frames)
{
  auto *f = (IdFilter *)ctx;
  for (int i = 0; i < frames; i++)
  {
    f->zL[slot] += 0.08 * (l[i] - f->zL[slot]); l[i] = f->zL[slot];
    f->zR[slot] += 0.08 * (r[i] - f->zR[slot]); r[i] = f->zR[slot];
  }
}
double tapEnergy[16];
void energyTap(void *, int slot, int, double *l, double *, int frames)
{
  for (int i = 0; i < frames; i++) tapEnergy[slot] += l[i] * l[i];
}
}  // namespace

int main()
{
  std::printf("voicetap_check — the per-voice seam (ADR-148)\n");

  const Cfg cfgs[] = {
    {"defaults", {}},
    {"voiceEnv", {{"voiceEnv", 1}, {"sustainL", 0.6}}},
    {"oversample", {{"oversample", 1}}},
    {"width-A", {{"superMode", 1}, {"width", 1.5}}},
    {"scatter+glide", {{"onsetScatter", 0.4}, {"freqGlide", 0.2}}},
  };

  for (const auto &cfg : cfgs)
  {
    hypersaw::SwarmCore a(kSR), b(kSR);
    drive(a, cfg); drive(b, cfg);
    b.setNoteTap(noopTap, nullptr);
    std::vector<float> aL(kFrames), aR(kFrames), bL(kFrames), bR(kFrames);
    renderAll(a, aL.data(), aR.data());
    renderAll(b, bL.data(), bR.data());
    const bool same = !std::memcmp(aL.data(), bL.data(), kFrames * 4)
                   && !std::memcmp(aR.data(), bR.data(), kFrames * 4);
    char buf[96];
    std::snprintf(buf, sizeof buf, "T1 no-op tap bit-identical [%s]", cfg.name);
    check(same, buf);
  }

  {
    hypersaw::SwarmCore a(kSR), b(kSR);
    drive(a, cfgs[1]); drive(b, cfgs[1]);
    IdFilter f{}; b.setNoteTap(identityTap, &f);
    std::vector<float> aL(kFrames), aR(kFrames), bL(kFrames), bR(kFrames);
    renderAll(a, aL.data(), aR.data());
    renderAll(b, bL.data(), bR.data());
    check(!std::memcmp(aL.data(), bL.data(), kFrames * 4)
          && !std::memcmp(aR.data(), bR.data(), kFrames * 4),
          "T2 unit-coefficient filter bit-identical");
  }

  {
    hypersaw::SwarmCore a(kSR), b(kSR);
    drive(a, cfgs[0]); drive(b, cfgs[0]);
    IdFilter f{}; b.setNoteTap(lpTap, &f);
    std::vector<float> aL(kFrames), aR(kFrames), bL(kFrames), bR(kFrames);
    renderAll(a, aL.data(), aR.data());
    renderAll(b, bL.data(), bR.data());
    double diff = 0;
    for (int i = 0; i < kFrames; i++) diff += std::fabs((double)aL[i] - bL[i]);
    char buf[64];
    std::snprintf(buf, sizeof buf, "(sum |delta| %.4f)", diff);
    check(diff > 0.01, "T3 a real filter audibly changes the output", buf);
  }

  {
    hypersaw::SwarmCore c(kSR);
    drive(c, cfgs[0]);
    std::memset(tapEnergy, 0, sizeof tapEnergy);
    c.setNoteTap(energyTap, nullptr);
    std::vector<float> L(kFrames), R(kFrames);
    renderAll(c, L.data(), R.data());
    int live = 0;
    for (double e : tapEnergy) if (e > 1e-6) live++;
    char buf[64];
    std::snprintf(buf, sizeof buf, "(%d live note buffers for 3 gated notes)", live);
    check(live == 3, "T4 per-note buffers carry exactly the gated notes", buf);
  }

  if (failures) std::printf("voicetap_check: RED (%d failure%s)\n", failures, failures == 1 ? "" : "s");
  else std::printf("voicetap_check: GREEN (0 failures)\n");
  return failures ? 1 : 0;
}
