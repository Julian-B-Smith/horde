/*
 * measure_alias — the aliasing figure for docs/MEASUREMENTS.md (ROADMAP B103).
 *
 * Drives the SHIPPED plugin through the CLAP factory (a core-direct probe
 * skips the shell — L0031), one oscillator, n=1, no detune/width/drift/K, and
 * renders a held note at MIDI 36 / 60 / 84 / 96 with the ADR-094 saw-shape
 * section engaged (sawProfile 130 = 0.5, round 131 = 0.6, roundHi 132 = 0.5)
 * and disengaged, in three modes — naive (digital=0), polyBLEP (digital=1),
 * polyBLEP + 2x oversample (id 88) — at 44.1 k and 96 k.
 *
 * TWO FIGURES per cell, "integral / worst":
 *   integral = 10*log10( energy in every bin farther than kExcl bins from any
 *              harmonic / energy within kExcl bins of the harmonics )
 *   worst    = max over the inter-harmonic midpoints (k+0.5)*f0 of
 *              20*log10(local peak / h1)
 * The integral is the "energy at non-harmonic bins over harmonic bins" the
 * brief asks for, and it is only honest because of the WINDOW: with a Hann
 * window that sum is window leakage from hundreds of harmonics and once
 * reported a clean polyBLEP saw at -27.7 dB instead of -149.5 dB (LIBRARY
 * L0016). A Kaiser window at beta 19 has sidelobes below about -180 dB and a
 * main lobe of ~6 bins, so with a +-16-bin exclusion around every harmonic
 * what remains in the "non-harmonic" set is fold-back or the floor — and the
 * two controls below are what say whether that holds at each note. Its blind
 * spot is declared, not hidden: an alias that lands within 16 bins of a
 * harmonic is counted as harmonic (at 96 k, 16 bins is 11.7 Hz).
 * The midpoint figure is the tree's calibrated protocol (L0016/L0017;
 * blep_alias_incommensurate_probe.cpp, whose FFT this reuses by copy). It
 * SAMPLES the alias comb where it happens to fall, so it depends on sr/f0
 * arithmetic — it ranks modes within a row; it is not an absolute.
 * Peaks are the local maximum in a small bin window (f0 is not on the bin
 * grid — MIDI notes are incommensurate with a 2^17 FFT — so the harmonic comb
 * is not bin-locked, and f0 is refined from the fundamental's peak so the
 * k-th harmonic window does not drift at high k).
 *
 * CALIBRATION (L0016/L0032 — a detector must be shown reading ~zero on a
 * known-clean signal and large on a known-bad one BEFORE its numbers mean
 * anything): two synthetic controls per note, generated in this file with no
 * plugin involved — an alias-free additive band-limited saw (must sit at the
 * floor) and a naive saw 2*frac-1 (must read large). They print first.
 *
 * Deterministic: the plugin's seeded streams only; the tool reads no clock.
 * Standalone, registered in CMake beside svf_check, NOT in ./verify.
 */
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
#include "build_stamp.h"   // generated every build (CMake target)

namespace
{
#include "notefuzz_scaffold.inc"

constexpr double kPi = 3.14159265358979323846;  // M_PI is undefined under MSVC (L0003)
constexpr int kLogN = 17;
constexpr int kN = 1 << kLogN;          // 131072 samples: 0.336 Hz bins at 44.1 k, 0.732 Hz at 96 k
constexpr double kSettleS = 1.0;        // attack is 3 ms; the shape section slews per tick — 1 s is ample

// Iterative radix-2 Cooley-Tukey, in place (copied from blep_alias_incommensurate_probe.cpp).
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
    const double ang = -2 * kPi / (double)len;
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

constexpr double kKaiserBeta = 19.0;   // sidelobes < -180 dB; main lobe ~6 bins
constexpr int kExcl = 16;              // bins either side of a harmonic counted AS harmonic

double besselI0(double x)   // series; converges in ~40 terms at beta 19
{
  double sum = 1, term = 1; const double q = x * x / 4;
  for (int m = 1; m < 200; m++) { term *= q / ((double)m * m); sum += term; if (term < sum * 1e-18) break; }
  return sum;
}

std::vector<double> spectrum(const std::vector<double> &x)
{
  std::vector<std::complex<double>> a(kN);
  const double norm = 1.0 / besselI0(kKaiserBeta);
  for (int i = 0; i < kN; i++)
  {
    const double r = 2.0 * i / (kN - 1) - 1.0;
    a[i] = x[i] * besselI0(kKaiserBeta * std::sqrt(std::max(0.0, 1 - r * r))) * norm;  // Kaiser
  }
  fft(a);
  std::vector<double> mag(kN / 2 + 1);
  for (int i = 0; i <= kN / 2; i++) mag[i] = std::abs(a[i]);
  return mag;
}

// Local peak within +-win bins of targetHz; *atBin receives where it sat.
double peakNear(const std::vector<double> &mag, double sr, double targetHz, int win, int *atBin = nullptr)
{
  const int c = (int)std::lround(targetHz * kN / sr);
  double best = 0; int bb = c;
  for (int b = c - win; b <= c + win; b++)
    if (b >= 0 && b < (int)mag.size() && mag[b] > best) { best = mag[b]; bb = b; }
  if (atBin) *atBin = bb;
  return best;
}

struct Alias { double integralDb, worstDb; int nMid; double f0Est; };

Alias measure(const std::vector<double> &x, double sr, double f0Nominal)
{
  auto mag = spectrum(x);
  const double binHz = sr / kN;
  // Refine f0: parabolic interpolation on log-magnitude around the fundamental's peak.
  int pb = 0; const double h1 = peakNear(mag, sr, f0Nominal, 10, &pb);
  double frac = 0;
  if (pb > 0 && pb + 1 < (int)mag.size())
  {
    const double l = std::log(std::max(mag[pb - 1], 1e-300)), c = std::log(std::max(mag[pb], 1e-300)),
                 r = std::log(std::max(mag[pb + 1], 1e-300));
    const double d = l - 2 * c + r;
    if (d != 0) frac = 0.5 * (l - r) / d;
  }
  const double f0 = (pb + frac) * binHz;
  const double nyq = sr / 2;
  // Worst midpoint, relative to h1.
  double worst = -1e9; int nMid = 0;
  for (int k = 1; k * f0 < nyq - f0; k++)
  {
    const double m = peakNear(mag, sr, (k + 0.5) * f0, 2);  // narrow: never reach a harmonic's skirt
    worst = std::max(worst, 20 * std::log10(std::max(m, 1e-300) / h1));
    nMid++;
  }
  // Integral: every bin is harmonic (within kExcl of some k*f0, k >= 1) or not.
  // The DC lobe (bins < kExcl) is neither and is skipped.
  double eh = 0, en = 0;
  for (int b = kExcl; b <= kN / 2; b++)
  {
    const double hz = b * binHz;
    const double k = std::round(hz / f0);
    const bool harmonic = k >= 1 && std::fabs(hz - k * f0) <= kExcl * binHz;
    (harmonic ? eh : en) += mag[b] * mag[b];
  }
  return {10 * std::log10(std::max(en, 1e-300) / std::max(eh, 1e-300)), worst, nMid, f0};
}

// ---- the shipped plugin, through the factory ------------------------------
struct Probe
{
  const clap_plugin_t *p = nullptr;
  std::vector<float> L, R;
  clap_audio_buffer_t out{};
  clap_process_t proc{};
  float *ch[2];
  void boot(double sr)
  {
    auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw");
    p->init(p); p->activate(p, sr, 32, kBlock); p->start_processing(p);
    L.assign(kBlock, 0); R.assign(kBlock, 0); ch[0] = L.data(); ch[1] = R.data();
    out.data32 = ch; out.channel_count = 2;
    proc.frames_count = kBlock; proc.audio_outputs = &out; proc.audio_outputs_count = 1; proc.out_events = &kOut;
  }
  void step(EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); }
  void set(const std::vector<std::pair<clap_id, double>> &kv)
  { EvList e; for (auto &x : kv) e.params.push_back(mkParam(x.first, x.second)); step(e); }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};

// kN steady-state samples of the left channel, after kSettleS of the held note.
std::vector<double> renderPlugin(double sr, int key, bool shape, double digital, double os)
{
  Probe pr; pr.boot(sr);
  std::vector<std::pair<clap_id, double>> kv = {
    {1, 1}, {4, 0}, {14, 0}, {6, 0}, {9, 0},   // n=1, detune 0, width 0, K 0, drift 0: one plain oscillator
    {16, digital}, {88, os},
    {130, shape ? 0.5 : 0}, {131, shape ? 0.6 : 0}, {132, shape ? 0.5 : 0}};
  pr.set(kv);
  { EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)key, 1, 0.9)); pr.step(e); }
  const int skip = (int)(kSettleS * sr);
  std::vector<double> o; o.reserve(skip + kN + kBlock);
  while ((int)o.size() < skip + kN)
  { EvList q; pr.step(q); for (int j = 0; j < kBlock; j++) o.push_back(pr.L[j]); }
  { EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_OFF, 0, (int16_t)key, 1, 0)); pr.step(e); }
  pr.kill();
  return std::vector<double>(o.begin() + skip, o.begin() + skip + kN);
}

// ---- controls: no plugin, generated here -----------------------------------
std::vector<double> additiveSaw(double sr, double f0)   // alias-free by construction
{
  std::vector<double> x(kN, 0.0);
  const int kmax = (int)((sr / 2 - 1.0) / f0);
  for (int k = 1; k <= kmax; k++)
    for (int i = 0; i < kN; i++) x[i] += std::sin(2 * kPi * k * f0 * i / sr) / k;
  return x;
}
std::vector<double> naiveSaw(double sr, double f0)
{
  std::vector<double> x(kN);
  for (int i = 0; i < kN; i++) { const double ph = std::fmod(f0 * i / sr, 1.0); x[i] = 2 * ph - 1; }
  return x;
}

double midiHz(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
}  // namespace

int main()
{
  hypersaw_entry_init("");
  const int keys[] = {36, 60, 84, 96};
  const double rates[] = {44100.0, 96000.0};

  std::printf("### Detector calibration (synthetic, no plugin)\n\n");
  std::printf("| note | f0 Hz | sr | additive band-limited saw: integral / worst (must read the floor) | naive saw: integral / worst (must read large) |\n");
  std::printf("|---|---|---|---|---|\n");
  for (double sr : rates)
    for (int k : keys)
    {
      const double f0 = midiHz(k);
      auto c = measure(additiveSaw(sr, f0), sr, f0);
      auto d = measure(naiveSaw(sr, f0), sr, f0);
      std::printf("| %d | %.2f | %.1fk | %.1f / %.1f dB (%d midpoints) | %.1f / %.1f dB |\n",
                  k, f0, sr / 1000, c.integralDb, c.worstDb, c.nMid, d.integralDb, d.worstDb);
    }

  std::printf("\n### The shipped oscillator (CLAP factory, n=1)\n\n");
  std::printf("| note | f0 Hz | sr | saw shape | naive: integral / worst | polyBLEP: integral / worst | polyBLEP + 2x OS: integral / worst |\n");
  std::printf("|---|---|---|---|---|---|---|\n");
  for (double sr : rates)
    for (int k : keys)
      for (int shape = 0; shape < 2; shape++)
      {
        const double f0 = midiHz(k);
        struct Mode { double digital, os; };
        const Mode modes[] = {{0, 0}, {1, 0}, {1, 1}};
        std::printf("| %d | %.2f | %.1fk | %s |", k, f0, sr / 1000, shape ? "on" : "off");
        for (const auto &m : modes)
        {
          auto r = measure(renderPlugin(sr, k, shape != 0, m.digital, m.os), sr, f0);
          std::printf(" %.1f / %.1f dB |", r.integralDb, r.worstDb);
        }
        std::printf("\n");
      }
  std::printf("\nBuild %s. FFT 2^%d, Kaiser beta %.0f, harmonic exclusion +-%d bins, %.0f s settle.\n",
              HYPERSAW_BUILD_STAMP, kLogN, kKaiserBeta, kExcl, kSettleS);
  hypersaw_entry_deinit();
  return 0;
}
