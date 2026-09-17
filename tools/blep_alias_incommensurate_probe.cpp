// blep_alias_incommensurate_probe — SCRATCH tool, not wired into CMakeLists or
// ./verify (ROADMAP B12). Re-measures BLEP aliasing suppression at an
// INCOMMENSURATE fundamental, fixing the protocol flaw in the
// 2026-08-01 "Clean-mode aliasing measured" pass (ROADMAP.md, "Clean-mode
// aliasing measured (2026-08-01)"): that run used bin-COMMENSURATE f0
// (E3=164.8, 660, 1763 Hz), and a commensurate saw's aliased partials land
// exactly ON the FFT bin grid too — the same grid the inter-harmonic-midpoint
// protocol (L0016/L0017) deliberately samples AWAY from. So the earlier
// BLEP "-180 dB" rows were the protocol seeing nothing, not the saw being
// that clean (ROADMAP explicitly flagged this and left it "still owed").
//
// METHOD (same core measurement idea as the 2026-08-01 pass, reused because
// it is sound; only f0 commensurability is fixed):
//   1. Render a single SwarmCore voice (n=1, no detune/width/drift/K) at a
//      given f0, digital=0 (naive) or digital=1 (BLEP), oversample on/off.
//   2. Take a long, steady-state, Hann-windowed block; FFT it.
//   3. h1 = the fundamental's peak magnitude (max within a small bin window
//      around f0, since an incommensurate f0 does not sit on an exact bin).
//   4. Sample the spectrum at the MIDPOINT between every pair of harmonics,
//      (k+0.5)*f0 for k=0..until Nyquist, taking the local peak in a small
//      window there too (same "peak near the target frequency" logic, not a
//      single fixed bin — necessary once the whole harmonic comb is no
//      longer bin-locked). Report worst and mean dB relative to h1.
//
// This is the aliasing residue at frequencies structurally AWAY from every
// harmonic (and, for an incommensurate f0, away from every aliased copy of
// a harmonic too, since aliases of harmonics are themselves at
// (fs*m +/- k*f0) which only coincide with (k'+0.5)*f0 by construction, not
// by grid coincidence) — it is the same "measure where the big thing is
// structurally absent" idea LIBRARY L0016 states.
//
// Standalone build (deliberately NOT added to CMakeLists.txt — brief scope):
//   clang++ -std=c++20 -O2 -I<repo>/src \
//     tools/blep_alias_incommensurate_probe.cpp \
//     -o /tmp/blep_alias_incommensurate_probe
#include <cstdio>
#include <cmath>
#include <complex>
#include <vector>
#include <string>
#include <algorithm>
#include "../src/swarm_core.h"
using namespace hypersaw;

namespace {

constexpr double kSR = 44100.0;
constexpr int kFFTLogN = 17;                 // N = 131072 -> bin width ~0.3364 Hz
constexpr int kFFTN = 1 << kFFTLogN;
constexpr int kSkip = 20000;                 // let envelope/gravity settle (matches waveshape_check's skip)

// Iterative radix-2 Cooley-Tukey FFT, in place. Self-contained (no external
// FFT dependency — ADR-002-style "reduce, never invent" would rather reuse
// something, but nothing FFT-shaped exists in this tree, and this is a
// scratch measurement tool, not a shipping oracle).
void fft(std::vector<std::complex<double>> &a)
{
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; i++)
  {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1)
  {
    const double ang = -2 * M_PI / (double)len;
    const std::complex<double> wlen(std::cos(ang), std::sin(ang));
    for (size_t i = 0; i < n; i += len)
    {
      std::complex<double> w(1);
      for (size_t k = 0; k < len / 2; k++)
      {
        auto u = a[i + k];
        auto v = a[i + k + len / 2] * w;
        a[i + k] = u + v;
        a[i + k + len / 2] = u - v;
        w *= wlen;
      }
    }
  }
}

// Render n=1 SwarmCore voice at f0 with the given digital/oversample setting,
// return kFFTN steady-state samples (post-skip).
std::vector<double> renderTail(double f0, double digital, double oversample)
{
  SwarmCore s(kSR);
  s.setParam("n", 1); s.setParam("detune", 0); s.setParam("width", 0);
  s.setParam("dist", 2); s.setParam("seed", 1234);
  s.setParam("K", 0); s.setParam("driftDepth", 0); s.setParam("retrig", 1);
  s.setParam("vol", 0.4);
  s.setParam("digital", digital);
  s.setParam("oversample", oversample);
  s.noteOn(38, f0);
  std::vector<float> L(512), R(512);
  std::vector<double> o;
  o.reserve(kSkip + kFFTN + 512);
  const int need = kSkip + kFFTN;
  while ((int)o.size() < need)
  { s.render(L.data(), R.data(), 512); for (int i = 0; i < 512; i++) o.push_back(L[i]); }
  return std::vector<double>(o.begin() + kSkip, o.begin() + kSkip + kFFTN);
}

// Magnitude spectrum (0..N/2) of a Hann-windowed render.
std::vector<double> spectrum(const std::vector<double> &x)
{
  std::vector<std::complex<double>> a(kFFTN);
  for (int i = 0; i < kFFTN; i++)
  {
    const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (kFFTN - 1));  // Hann
    a[i] = x[i] * w;
  }
  fft(a);
  std::vector<double> mag(kFFTN / 2 + 1);
  for (int i = 0; i <= kFFTN / 2; i++) mag[i] = std::abs(a[i]);
  return mag;
}

// Peak magnitude within +-winBins of the bin nearest targetHz.
double peakNear(const std::vector<double> &mag, double targetHz, int winBins)
{
  const double binHz = kSR / kFFTN;
  const int center = (int)std::lround(targetHz / binHz);
  double best = 0;
  for (int b = center - winBins; b <= center + winBins; b++)
    if (b >= 0 && b < (int)mag.size()) best = std::max(best, mag[b]);
  return best;
}

struct AliasResult { double worstDb, meanDb; int nMidpoints; };

AliasResult measureAlias(double f0, double digital, double oversample)
{
  auto x = renderTail(f0, digital, oversample);
  auto mag = spectrum(x);
  const double h1 = peakNear(mag, f0, 5);   // fundamental peak, small search window
  const double nyq = kSR / 2.0;
  double worst = -1e9, sum = 0;
  int n = 0;
  // Midpoints (k+0.5)*f0 for k = 0..until just under Nyquist, i.e. between
  // harmonic k and k+1 for every harmonic pair the render can hold.
  for (int k = 0; ; k++)
  {
    const double mid = (k + 0.5) * f0;
    if (mid >= nyq - f0) break;   // stop before the top harmonic pair is truncated by Nyquist
    const double m = peakNear(mag, mid, 2);   // narrower window: don't reach into the neighbouring harmonic's skirt
    const double db = 20.0 * std::log10(std::max(m, 1e-300) / h1);
    worst = std::max(worst, db);
    sum += db;
    n++;
  }
  return {worst, n > 0 ? sum / n : 0.0, n};
}

}  // namespace

int main()
{
  struct F0 { const char *label; double hz; bool commensurate; };
  // Commensurate control: bin index 2000 of the N=131072 FFT grid, i.e.
  // f0 * N / sr is an exact integer -> every harmonic AND every aliased
  // fold of it lands on an exact bin (the flaw under test).
  const double binHz = kSR / kFFTN;
  const F0 f0s[] = {
    {"commensurate ~673 Hz (bin-exact, reproduces the old protocol's blind spot)", 2000 * binHz, true},
    {"incommensurate 441.3 Hz",  441.3,  false},
    {"incommensurate 1760.3 Hz", 1760.3, false},
  };

  std::printf("f0 label | mode | os | h1-relative aliasing worst/mean dB (n midpoints)\n");
  std::printf("---|---|---|---\n");
  for (const auto &f : f0s)
  {
    struct Mode { const char *name; double digital, oversample; };
    const Mode modes[] = {
      {"naive (digital=0)", 0.0, 0.0},
      {"BLEP (digital=1)",  1.0, 0.0},
      {"BLEP + 2x OS",      1.0, 1.0},
    };
    for (const auto &m : modes)
    {
      auto r = measureAlias(f.hz, m.digital, m.oversample);
      std::printf("%-70s | %-18s | os=%d | worst=%8.2f dB  mean=%8.2f dB  (n=%d)\n",
                  f.label, m.name, m.oversample > 0.5, r.worstDb, r.meanDb, r.nMidpoints);
    }
  }
  return 0;
}
