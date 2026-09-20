/*
 * subosc_check — the SUB OSC port's oracle (B155 port phase 1).
 *
 * WHAT IT GATES
 *  1. L0-1 parity: `hypersaw::SubOscCore` reproduces reference/subosc.html's
 *     `SubOscCore` to <= 1e-6 RMS on every scenario, at BOTH 48 000 and
 *     44 100 Hz. Goldens from tools/golden/gen_subosc_goldens.mjs, which slices
 *     the lab live. There is no monitoring stage to invert here (see that
 *     file's header), so parity is measured at full engine amplitude.
 *  2. SPEC-SUBOSC §10's MEASURED rows, re-measured against the C++ core with
 *     their must-fail controls: determinism (§10.2), sample-rate independence
 *     (§10.3), block-size independence (§10.4), exact silence and a
 *     subnormal-free tail (§10.5), the bump's two-lobe silhouette and its
 *     peak-exact normaliser (§10.6), plus §5.1's rate-warp finding (audit A7)
 *     and §7's clamped-table contract (audit A2).
 *  3. A CPU number. REPORTED, never gated: the absolute ratio is
 *     machine-dependent and SPEC-SUBOSC §10.7 claims no budget ("CPU per voice
 *     — still to measure before a port"). This is that measurement.
 *
 * WIRED in `./verify full` (ADR-180 §1: a check is wired in the PR that creates
 * it), beside station's chain: generator --selfcheck, generator, then this
 * binary with the golden dir as argv[1].
 *
 * EVERY ROW CARRIES A CONTROL that must read the other answer (L0016/L0032).
 * Where the lab harness plants a defective build to get its must-fail control
 * (tools/labharness/subosc_check.mjs, also wired in `verify full`), C++ cannot;
 * the equivalent here is a row that asserts BOTH directions — the quantity
 * under test AND a deliberately-wrong hypothesis the same detector must reject,
 * evaluated through the core's own public surface (`flushFloor` is the one test
 * hook, the StationCore::pmConst idiom).
 *
 * THE ONE §10 ROW THAT IS NOT HERE, AND WHY. §10.1's aliasing floors are a
 * 65 536-point Kaiser-windowed FFT sweep for the worst inharmonic bin across
 * six shapes and four notes. No FFT harness exists on the C++ side, and the
 * property is a fact about the SHAPE LAWS, which parity pins bit-for-bit — an
 * alias regression in this port cannot happen without a parity failure first,
 * and the floors themselves are gated at the lab by subosc_check.mjs, which
 * `./verify full` now runs. What IS here instead is a narrow, Goertzel-based
 * confirmation that the polyBLEP correction is present in THIS build, with a
 * naive saw as its control: comparable to §10.1's numbers, not identical with
 * them (the same boundary station_check records for its DRW alias row).
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../src/subosc_core.h"

using hypersaw::SubOscCore;

namespace
{
int g_failures = 0;

void row(bool ok, const char *label, const char *fmt = nullptr, double v = 0)
{
  std::printf("%s %s", ok ? "PASS" : "FAIL", label);
  if (fmt) { std::printf("  "); std::printf(fmt, v); }
  std::printf("\n");
  if (!ok) g_failures++;
}

void head(const char *s) { std::printf("\n== %s ==\n", s); }

constexpr double kTwoPi = 6.283185307179586;
constexpr double kPi = 3.141592653589793;
// The smallest NORMAL float32. Anything below it and non-zero is a subnormal —
// the contract the host sees (§10.5, limit L5: this is not evidence about CPU
// stalls, which this machine cannot show either way).
constexpr double kF32MinNormal = 1.1754943508222875e-38;

double mtof(double m) { return 440 * std::pow(2, (m - 69) / 12); }

double rms(const std::vector<float> &a)
{
  double s = 0;
  for (float v : a) s += (double)v * v;
  return std::sqrt(s / std::max<size_t>(1, a.size()));
}

// ---------------------------------------------------------------- the core --
struct Set
{
  SubOscCore::Param p;
  double v;
};

SubOscCore make(double sr, std::initializer_list<Set> sets)
{
  SubOscCore c(sr);
  for (const Set &s : sets) c.setParam(s.p, s.v);
  return c;
}

// Mono left channel; `master` is the whole buffer (the core reads [0, n)).
std::vector<float> pull(SubOscCore &c, int n, const float *master = nullptr)
{
  std::vector<float> out((size_t)n), r((size_t)n);
  c.render(out.data(), r.data(), n, master);
  return out;
}

// First index at which two buffers differ, or -1. Bit comparison, not epsilon:
// every determinism and block-size row here claims EQUALITY, and an epsilon
// would quietly convert those claims into weaker ones.
long firstDiff(const std::vector<float> &a, const std::vector<float> &b)
{
  const size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; i++)
    if (a[i] != b[i]) return (long)i;
  return a.size() == b.size() ? -1 : (long)n;
}

// Spread of a set of measurements as a percentage of their mean — the lab
// harness's `spread`, so the two suites' drift numbers are comparable.
double spread(const std::vector<double> &xs)
{
  double lo = xs[0], hi = xs[0], sum = 0;
  for (double x : xs) { lo = std::min(lo, x); hi = std::max(hi, x); sum += x; }
  return (hi - lo) / (sum / (double)xs.size()) * 100;
}

// Goertzel: the magnitude of ONE frequency. A Hann window keeps the skirt of
// the (enormous) fundamental off the (tiny) alias bins being read.
double goertzel(const std::vector<float> &x, double sr, double f)
{
  const double w = kTwoPi * f / sr, c = 2 * std::cos(w);
  double s1 = 0, s2 = 0;
  const size_t N = x.size();
  for (size_t i = 0; i < N; i++)
  {
    const double win = 0.5 - 0.5 * std::cos(kTwoPi * (double)i / (double)(N - 1));
    const double s0 = (double)x[i] * win + c * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) * 2 / (double)N;
}

// ------------------------------------------------------------------ parity --
struct Scenario
{
  std::string name;
  double sr = 48000, secs = 0.5, vel = 1, master = -1;
  int block = 128, offAt = -1, note = 36;
  std::vector<std::pair<std::string, double>> keys;
};

// The master phase a sync scenario states: a saw at an absolute frequency,
// accumulated in double and stored f32 — the generator's `masterPhase`, the
// same two operations in the same order, which is what makes a sync reset land
// on the same sample in both languages.
std::vector<float> masterPhase(double hz, double sr, int n)
{
  std::vector<float> a((size_t)n);
  const double d = hz / sr;
  double ph = 0;
  for (int i = 0; i < n; i++)
  {
    ph += d;
    ph -= std::floor(ph);
    a[(size_t)i] = (float)ph;
  }
  return a;
}

// Interleaved stereo, rendered exactly as the generator does: BLOCK chunks with
// the note-off split onto its own boundary.
std::vector<float> renderScenario(const Scenario &sc, bool *keyOk = nullptr)
{
  SubOscCore c(sc.sr);
  bool ok = true;
  for (const auto &kv : sc.keys)
  {
    const int i = SubOscCore::indexOf(kv.first.c_str());
    // An UNKNOWN KEY IS A FAILURE, not a shrug: it means the lab grew a
    // parameter this port does not carry, which is exactly the silent parity
    // hole the whole-table dump exists to prevent.
    if (i < 0)
    {
      ok = false;
      std::printf("     unknown manifest key '%s'\n", kv.first.c_str());
      continue;
    }
    c.setParam((SubOscCore::Param)i, kv.second);
  }
  if (keyOk) *keyOk = ok;
  c.noteOn(sc.note, sc.vel);

  const int total = (int)std::lround(sc.sr * sc.secs);
  std::vector<float> master;
  if (sc.master > 0) master = masterPhase(sc.master, sc.sr, total);
  std::vector<float> out((size_t)total * 2);
  std::vector<float> L((size_t)sc.block), R((size_t)sc.block);
  for (int off = 0; off < total;)
  {
    int k = std::min(sc.block, total - off);
    if (sc.offAt > off && sc.offAt < off + k) k = sc.offAt - off;
    if (off == sc.offAt) c.noteOff();
    c.render(L.data(), R.data(), k, master.empty() ? nullptr : master.data() + off);
    for (int i = 0; i < k; i++)
    {
      out[(size_t)(off + i) * 2] = L[(size_t)i];
      out[(size_t)(off + i) * 2 + 1] = R[(size_t)i];
    }
    off += k;
  }
  return out;
}

// ------------------------------------------------------------- §10.6 lobes --
// The lobes are read from a RISING ZERO CROSSING, not from wherever the render
// window happens to start: without that anchor the "first" maximum is whichever
// one the buffer opened on, and the ordering claim silently inverts at some
// notes (it did, at MIDI 48, in the lab's first draft of this measurement).
struct Lobes
{
  double f0 = 0, per = 0, amax = 0;
  std::vector<double> pk, tr;
};

Lobes bumpLobes(double sr, int midi, double a, double phi)
{
  SubOscCore c = make(sr, {{SubOscCore::kWave, SubOscCore::kBump},
                           {SubOscCore::kOctave, 0},
                           {SubOscCore::kKeytrack, 1},
                           {SubOscCore::kLevel, 1},
                           {SubOscCore::kTone, 20000},
                           {SubOscCore::kAttack, 0.0005},
                           {SubOscCore::kRelease, 0.01},
                           {SubOscCore::kPhase, 0},
                           {SubOscCore::kBumpAmt, a},
                           {SubOscCore::kBumpPhase, phi}});
  c.noteOn(midi, 1);
  pull(c, 4096);  // past the attack ramp
  Lobes r;
  r.f0 = c.freqHz();
  r.per = sr / r.f0;
  const std::vector<float> y = pull(c, (int)std::ceil(3 * r.per) + 4);
  long z = -1;
  for (size_t i = 1; i < y.size(); i++)
    if (y[i - 1] <= 0 && y[i] > 0) { z = (long)i; break; }
  const long end = std::min((long)y.size() - 1, z + (long)std::lround(r.per));
  for (long i = z + 1; i < end; i++)
  {
    if (y[i] > 0 && y[i] > y[i - 1] && y[i] >= y[i + 1]) r.pk.push_back(y[i]);
    if (y[i] > 0 && y[i] < y[i - 1] && y[i] <= y[i + 1]) r.tr.push_back(y[i]);
  }
  for (float v : y) r.amax = std::max(r.amax, (double)std::fabs(v));
  return r;
}

// §10.6's (a, phi) grid, the lab's exactly — extremes a = 0 / a = 0.6 and
// phi = +-pi are in it deliberately.
const double kAs[] = {0, 0.1, 0.2, 0.3, 0.35, 0.4, 0.5, 0.6};
const double kPhis[] = {-kPi, -2, -1, -0.25, 0, 0.25, 1, 2, kPi};

// The peak of the SHAPE (not of a render) by brute-force scan, against the
// normaliser the core's own recalc() computed for those parameters. The scan is
// the INDEPENDENT half: brute force over the raw formula where the core uses a
// 32-bracket bisection search, so agreement is two different methods meeting,
// not one method agreeing with itself. Scan bias is bounded by
// |f''|/2 * (dth/2)^2 <= 6.4/2 * (4.8e-5)^2 = 7e-9, two orders under the 1e-6
// tolerance. `norm` = 0 means "ask the core", anything else plants a normaliser.
constexpr int kPeakScan = 65536;
double shapePeak(double a, double phi, double norm = 0)
{
  if (norm == 0)
  {
    SubOscCore c = make(44100, {{SubOscCore::kBumpAmt, a}, {SubOscCore::kBumpPhase, phi}});
    norm = 1 / SubOscCore::bumpPeak(c.param(SubOscCore::kBumpAmt), c.param(SubOscCore::kBumpPhase));
  }
  double m = 0;
  for (int i = 0; i < kPeakScan; i++)
    m = std::max(m, std::fabs(SubOscCore::bumpAt((double)i / kPeakScan, a, phi, norm)));
  return m;
}
}  // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/subosc";

  // ================================================================ 1. parity
  std::ifstream mf(dir + "/subosc-manifest.tsv");
  if (!mf)
  {
    std::printf("FAIL subosc manifest missing (run tools/golden/gen_subosc_goldens.mjs)\n");
    return 1;
  }
  std::vector<Scenario> scenarios;
  std::string line;
  while (std::getline(mf, line))
  {
    if (line.empty() || line[0] == '#') continue;
    const auto tab = line.find('\t');
    Scenario sc;
    sc.name = line.substr(0, tab);
    std::istringstream ss(line.substr(tab + 1));
    std::string tok;
    while (ss >> tok)
    {
      const auto eq = tok.find('=');
      const std::string k = tok.substr(0, eq), v = tok.substr(eq + 1);
      if (k == "@sr") sc.sr = std::atof(v.c_str());
      else if (k == "@secs") sc.secs = std::atof(v.c_str());
      else if (k == "@block") sc.block = std::atoi(v.c_str());
      else if (k == "@off") sc.offAt = std::atoi(v.c_str());
      else if (k == "@master") sc.master = std::atof(v.c_str());
      else if (k == "@note")
      {
        const auto c1 = v.find(':');
        sc.note = std::atoi(v.c_str());
        sc.vel = std::atof(v.c_str() + c1 + 1);
      }
      else sc.keys.push_back({k, std::atof(v.c_str())});
    }
    scenarios.push_back(sc);
  }
  std::printf("-- parity (L0-1, eps = 1e-6 RMS) -- %zu scenarios at 48 000 and 44 100 Hz --\n",
              scenarios.size());

  double worst = 0;
  for (const auto &sc : scenarios)
  {
    std::ifstream gf(dir + "/" + sc.name + ".f32", std::ios::binary);
    if (!gf)
    {
      row(false, sc.name.c_str(), "golden missing");
      continue;
    }
    std::vector<float> ref;
    gf.seekg(0, std::ios::end);
    ref.resize((size_t)gf.tellg() / 4);
    gf.seekg(0);
    gf.read((char *)ref.data(), (std::streamsize)ref.size() * 4);

    bool keyOk = true;
    const std::vector<float> got = renderScenario(sc, &keyOk);
    double acc = 0, era = 0;
    const size_t n = std::min(ref.size(), got.size());
    for (size_t i = 0; i < n; i++)
    {
      const double d = (double)got[i] - (double)ref[i];
      acc += d * d;
      era += (double)ref[i] * ref[i];
    }
    const double r = std::sqrt(acc / std::max<size_t>(1, n));
    const double refRms = std::sqrt(era / std::max<size_t>(1, n));
    worst = std::max(worst, r);
    char buf[192];
    std::snprintf(buf, sizeof buf, "parity %-18s (ref rms %.4f, %zu samples)  rms =",
                  sc.name.c_str(), refRms, n);
    // refRms > 1e-4 is not decoration: a golden of silence would pass an RMS
    // comparison against a port that also renders silence, which is how a
    // scenario stops testing anything without anyone noticing.
    row(keyOk && ref.size() == got.size() && r <= 1e-6 && refRms > 1e-4, buf, "%.3e", r);
  }
  std::printf("   worst parity rms over all scenarios: %.3e  (gate 1e-6)\n", worst);

  // ========================================================= 2. §10.2 determinism
  head("§10.2 determinism — the seed is the control, and it reaches the stream");
  {
    auto noisy = [](uint32_t seed) {
      return make(44100, {{SubOscCore::kWave, SubOscCore::kNoise},
                          {SubOscCore::kTone, 1500},
                          {SubOscCore::kSeed, (double)seed}});
    };
    SubOscCore a = noisy(1), b = noisy(1);
    a.noteOn(36, 1);
    b.noteOn(36, 1);
    const std::vector<float> ya = pull(a, 16384), yb = pull(b, 16384);
    row(firstDiff(ya, yb) == -1 && rms(ya) > 1e-3,
        "two instances, same seed -> bit-identical (16384 samples)");

    // Five notes of history, then allOff(), must be indistinguishable from a
    // fresh core: D3's claim is that NO stream survives the reset.
    SubOscCore h = noisy(1);
    for (int i = 0; i < 5; i++)
    {
      h.noteOn(40 + i, 0.6);
      pull(h, 3000);
      h.noteOff();
      pull(h, 2000);
    }
    SubOscCore hk = h;  // the same history, WITHOUT the reset — the control
    h.allOff();
    h.noteOn(36, 1);
    const std::vector<float> yh = pull(h, 16384);
    row(firstDiff(ya, yh) == -1, "five notes of history + allOff() -> bit-identical to fresh");

    hk.noteOn(36, 1);
    const std::vector<float> yk = pull(hk, 16384);
    row(firstDiff(ya, yk) != -1,
        "CONTROL history WITHOUT allOff must differ (env/filter carry, by design)",
        "first diff at %.0f", (double)firstDiff(ya, yk));

    SubOscCore d = noisy(9876543);
    d.noteOn(36, 1);
    const std::vector<float> yd = pull(d, 16384);
    row(firstDiff(ya, yd) == 0, "CONTROL different seed must differ from sample 0",
        "first diff at %.0f", (double)firstDiff(ya, yd));

    // The bump draws on no stream, so its determinism claim is the narrower
    // one: the output is a pure function of (parameters, note). Its must-differ
    // control is therefore the phase, not a seed.
    auto bumper = [](double phi) {
      return make(44100, {{SubOscCore::kWave, SubOscCore::kBump}, {SubOscCore::kBumpPhase, phi}});
    };
    SubOscCore p1 = bumper(-0.25), p2 = bumper(-0.25), p3 = bumper(-0.24);
    p1.noteOn(36, 1);
    p2.noteOn(36, 1);
    p3.noteOn(36, 1);
    const std::vector<float> y1 = pull(p1, 16384), y2 = pull(p2, 16384), y3 = pull(p3, 16384);
    row(firstDiff(y1, y2) == -1 && rms(y1) > 1e-3,
        "bump: two instances, same params -> bit-identical");
    row(firstDiff(y1, y3) == 0, "CONTROL bumpPhase -0.25 vs -0.24 must differ from sample 0",
        "first diff at %.0f", (double)firstDiff(y1, y3));
  }

  // =============================================== 3. §10.3 sample-rate independence
  head("§10.3 sample-rate independence, 44.1 / 48 / 96 kHz");
  const std::vector<double> kRates = {44100, 48000, 96000};
  {
    // (a) envelope attack: time to env = 0.9, rendered one sample at a time so
    // the observable is read at sample resolution, the crossing interpolated.
    auto attackSeconds = [](double sr) {
      SubOscCore c = make(sr, {{SubOscCore::kAttack, 0.05},
                               {SubOscCore::kLevel, 1},
                               {SubOscCore::kWave, SubOscCore::kSine}});
      c.noteOn(60, 1);
      float l, r;
      double prev = 0;
      const int n = (int)std::ceil(0.2 * sr);
      for (int i = 0; i < n; i++)
      {
        c.render(&l, &r, 1);
        if (c.env >= 0.9)
        {
          const double frac = (0.9 - prev) / (c.env - prev);
          return (i + frac) / sr;
        }
        prev = c.env;
      }
      return std::nan("");
    };
    std::vector<double> t;
    for (double sr : kRates)
    {
      t.push_back(attackSeconds(sr));
      std::printf("   attack to 0.9 @ %.0f = %.4f ms\n", sr, t.back() * 1000);
    }
    row(spread(t) <= 0.5, "attack time drift <= 0.5 %", "%.4f %%", spread(t));

    // CONTROL — the ADR-009 trap itself, and the reason every time constant in
    // this core is a number of SECONDS: the same detector, run over a ramp
    // whose per-sample increment was fixed once at 44.1 kHz, must drift wildly.
    // The plant is arithmetic here rather than a mutated build (C++ cannot
    // plant a string into its own source the way the lab harness does), which
    // is legitimate because the quantity it corrupts — the increment — is the
    // whole of the mechanism under test.
    std::vector<double> tc;
    const double fixedInc = 1.0 / (0.05 * 44100);
    for (double sr : kRates)
    {
      double e = 0;
      int i = 0;
      for (; e < 0.9 && i < (int)(0.5 * sr); i++) e += fixedInc;
      tc.push_back(i / sr);
    }
    std::printf("   control (increment fixed at 44.1 k): %.1f / %.1f / %.1f ms\n",
                tc[0] * 1000, tc[1] * 1000, tc[2] * 1000);
    row(spread(tc) > 10, "CONTROL per-sample-constant attack must drift > 10 %", "%.2f %%", spread(tc));

    // (b) tone time constant, in SECONDS, measured from the decay RATIO at two
    // points well past the edge. A ratio of two late samples cancels the
    // one-sample BLEP smear at the edge, which is itself rate-dependent and
    // would otherwise be the thing measured.
    auto toneTau = [](double sr) {
      SubOscCore c = make(sr, {{SubOscCore::kWave, SubOscCore::kSquare},
                               {SubOscCore::kKeytrack, 0},
                               {SubOscCore::kOctave, -2},
                               {SubOscCore::kTone, 200},
                               {SubOscCore::kLevel, 1},
                               {SubOscCore::kAttack, 0.0005},
                               {SubOscCore::kPhase, 0}});
      c.noteOn(60, 1);
      const std::vector<float> y = pull(c, (int)std::ceil(0.06 * sr));
      const double f0 = c.freqHz();
      const long edge = std::lround(0.5 / f0 * sr);             // the square's falling edge
      const double settled = y[(size_t)(edge + std::lround(0.020 * sr))];
      const double tau0 = 1 / (kTwoPi * 200);
      const long i1 = edge + std::lround(2 * tau0 * sr), i2 = edge + std::lround(6 * tau0 * sr);
      const double d1 = std::fabs(y[(size_t)i1] - settled), d2 = std::fabs(y[(size_t)i2] - settled);
      return ((i2 - i1) / sr) / std::log(d1 / d2);
    };
    std::vector<double> tt;
    for (double sr : kRates)
    {
      tt.push_back(toneTau(sr));
      std::printf("   tone tau (fc 200 Hz) @ %.0f = %.2f us   (ideal 1/2pi.fc = %.2f us)\n",
                  sr, tt.back() * 1e6, 1e6 / (kTwoPi * 200));
    }
    row(spread(tt) <= 0.5, "tone time constant drift <= 0.5 %", "%.4f %%", spread(tt));

    // (c) audit A7's actual finding: does the tone's MAGNITUDE response move
    // with the sample rate? A time constant can be right while the response
    // warps. octave 0 EXPLICITLY — the default is -1 and the closed-form anchor
    // below is written in terms of mtof(83).
    auto toneMagRatio = [](double sr) {
      auto at = [sr](double fc) {
        SubOscCore c = make(sr, {{SubOscCore::kWave, SubOscCore::kSine},
                                 {SubOscCore::kTone, fc},
                                 {SubOscCore::kLevel, 1},
                                 {SubOscCore::kAttack, 0.0005},
                                 {SubOscCore::kKeytrack, 1},
                                 {SubOscCore::kOctave, 0}});
        c.noteOn(83, 1);  // 987.77 Hz, the same note at every rate
        pull(c, (int)std::lround(0.05 * sr));
        return rms(pull(c, (int)std::lround(0.2 * sr)));
      };
      return at(500) / at(20000);
    };
    std::vector<double> mag, closed;
    for (double sr : kRates)
    {
      mag.push_back(toneMagRatio(sr));
      // Closed-form anchor: the TPT one-pole's magnitude is g/sqrt(g^2+tan^2),
      // an oracle that needs no reference implementation at all (L0031).
      const double g = std::tan(kPi * 500 / sr), t = std::tan(kPi * mtof(83) / sr);
      closed.push_back(g / std::sqrt(g * g + t * t));
      std::printf("   |H(987.8 Hz)| fc 500 @ %.0f = %.6f   closed form %.6f\n",
                  sr, mag.back(), closed.back());
    }
    row(spread(mag) <= 0.5, "tone magnitude drift <= 0.5 %", "%.4f %%", spread(mag));
    double err = 0;
    for (size_t i = 0; i < mag.size(); i++)
      err = std::max(err, std::fabs(mag[i] - closed[i]) / closed[i] * 100);
    row(err < 1, "tone magnitude matches closed form within 1 %", "worst %.4f %%", err);

    // (d) D4 made visible — audit A7 / §3.6: with the naive pole
    // `1 - exp(-2pi*fc/sr)`, "tone wide open" IS A DIFFERENT FILTER AT EVERY
    // SAMPLE RATE. Read at Nyquist with the tone at its §7 default of 20 kHz,
    // where the defect is largest and where the TPT has a structural zero — the
    // bilinear map sends s = inf to z = -1, so the ported law reads exactly 0
    // at 44.1, 48 AND 96 kHz while the naive one moves by more than a dB.
    //
    // THIS ROW COMPARES TWO LAWS IN CLOSED FORM, not two builds: the core's
    // adherence to the TPT law is row (c)'s business (measured against the same
    // closed form, worst error 0.108 %), and the naive law has no build here to
    // run — its whole point is that this port does not contain it. It is the
    // CONTROL for row (c): a detector that showed no difference between the two
    // laws anywhere would make row (c)'s 0.16 % drift evidence of nothing. The
    // audit's own figures (-1.34 dB at 44.1 k vs -5.53 dB at 96 k) were taken
    // on the SAW engine's filter at its own setting; these are the same class
    // of defect re-measured at SUB OSC's default, not a reproduction of them.
    std::vector<double> naive;
    for (double sr : kRates)
    {
      const double a = 1 - std::exp(-kTwoPi * 20000 / sr);  // the naive coefficient
      // |H| of y += a*(x - y) at Nyquist (w = pi) is a/(2 - a).
      naive.push_back(a / (2 - a));
      std::printf("   control naive one-pole |H(Nyquist)| fc 20000 @ %.0f = %.6f (%.2f dB); "
                  "the TPT law reads exactly 0 at every rate\n",
                  sr, naive.back(), 20 * std::log10(naive.back()));
    }
    row(spread(naive) > 10,
        "CONTROL the naive one-pole's 'wide open' drifts with the sample rate (audit A7)",
        "%.2f %%", spread(naive));

    // (e) the envelope reaches EXACTLY 1 in exactly `attack` seconds at every
    // rate — §5.3's whole argument for a linear ramp over an exponential one.
    // The tolerance is ONE SAMPLE, which is the quantisation of the claim and
    // not a fudge: `atkInc = 1/(attack*sr)` needs ceil(attack*sr) steps.
    std::vector<double> full;
    for (double sr : kRates)
    {
      SubOscCore c = make(sr, {{SubOscCore::kAttack, 0.05}, {SubOscCore::kLevel, 1}});
      c.noteOn(60, 1);
      float l, r;
      int i = 0;
      for (; i < (int)(0.2 * sr) && c.env < 1.0; i++) c.render(&l, &r, 1);
      full.push_back(i / sr);
      std::printf("   envelope reaches exactly 1.0 @ %.0f after %.5f ms (attack 50 ms, %d samples)\n",
                  sr, full.back() * 1000, i);
    }
    double wErr = 0;
    for (size_t i = 0; i < full.size(); i++)
      wErr = std::max(wErr, std::fabs(full[i] - 0.05) * kRates[i]);
    row(wErr <= 1.5 && spread(full) <= 0.5,
        "envelope hits exactly 1.0 at `attack` seconds, every rate (within 1 sample)",
        "worst error %.2f samples", wErr);
  }

  // ============================================ 4. §10.4 block-size independence
  head("§10.4 block-size independence, chunks 1 / 7 / 64 / 256 / 333 vs one whole render");
  {
    constexpr int kTotal = 20000;
    const std::vector<float> master = masterPhase(220, 44100, kTotal);
    // `reset` emulates the defect class this row exists to catch: a core whose
    // state does not survive a render call (audit A10, the SAW engine's pan
    // motion). It is the control, and it must break the comparator.
    auto chunked = [&](int leg, int chunk, bool reset) {
      SubOscCore c = leg == 0
          ? make(44100, {{SubOscCore::kWave, SubOscCore::kPulse},
                         {SubOscCore::kWidth, 0.27},
                         {SubOscCore::kTone, 900},
                         {SubOscCore::kLevel, 0.9},
                         {SubOscCore::kSync, 1},
                         {SubOscCore::kAttack, 0.004},
                         {SubOscCore::kRelease, 0.05}})
          : make(44100, {{SubOscCore::kWave, SubOscCore::kNoise},
                         {SubOscCore::kTone, 1500},
                         {SubOscCore::kLevel, 0.9},
                         {SubOscCore::kSeed, 7},
                         {SubOscCore::kAttack, 0.002}});
      c.noteOn(45, 1);
      std::vector<float> out((size_t)kTotal), L((size_t)chunk), R((size_t)chunk);
      for (int i = 0; i < kTotal;)
      {
        const int n = std::min(chunk, kTotal - i);
        if (reset && i > 0) { c.allOff(); c.noteOn(45, 1); }
        c.render(L.data(), R.data(), n, leg == 0 ? master.data() + i : nullptr);
        for (int k = 0; k < n; k++) out[(size_t)(i + k)] = L[(size_t)k];
        i += n;
      }
      return out;
    };
    const char *legName[2] = {"pulse + tone + hard sync", "seeded noise + tone"};
    for (int leg = 0; leg < 2; leg++)
    {
      const std::vector<float> ref = chunked(leg, kTotal, false);
      long bad = -1;
      int badChunk = 0;
      for (int ch : {1, 7, 64, 256, 333})
      {
        const long d = firstDiff(ref, chunked(leg, ch, false));
        if (d != -1 && bad == -1) { bad = d; badChunk = ch; }
      }
      char buf[128];
      std::snprintf(buf, sizeof buf, "block-size independent: %s", legName[leg]);
      if (bad == -1) row(true, buf, "all 5 chunkings bit-identical");
      else { std::printf("     chunk %d differs at %ld\n", badChunk, bad); row(false, buf); }
    }
    const std::vector<float> ref = chunked(0, kTotal, true);
    const long d = firstDiff(ref, chunked(0, 64, true));
    row(d != -1, "CONTROL a core reset between calls must break block independence",
        "first diff at %.0f", (double)d);
  }

  // ====================================== 5. §10.5 exact silence and denormals
  head("§10.5 level 0 is silence exactly, and the release tail never goes subnormal");
  {
    // The count comes from the enum, not from a literal: a seventh shape was
    // added (bump, B155) and a hardcoded "six" would have made this row lie
    // about its own coverage while still passing.
    long bad = -1;
    int badWave = -1;
    for (int w = 0; w < SubOscCore::kWaveCount; w++)
    {
      SubOscCore c = make(44100, {{SubOscCore::kWave, (double)w},
                                  {SubOscCore::kLevel, 0},
                                  {SubOscCore::kTone, 900},
                                  {SubOscCore::kAttack, 0.001}});
      c.noteOn(36, 1);
      const std::vector<float> y = pull(c, 8192);
      for (size_t i = 0; i < y.size(); i++)
        if (y[i] != 0) { bad = (long)i; badWave = w; break; }
      if (bad != -1) break;
    }
    char buf[128];
    std::snprintf(buf, sizeof buf, "all %d waveforms, level 0 -> every sample exactly 0",
                  SubOscCore::kWaveCount);
    if (bad == -1) row(true, buf, "7 waveforms x 8192 samples");
    else { std::printf("     wave %d non-zero at %ld\n", badWave, bad); row(false, buf); }

    SubOscCore c = make(44100, {{SubOscCore::kWave, SubOscCore::kSaw},
                                {SubOscCore::kLevel, 0.001},
                                {SubOscCore::kTone, 900},
                                {SubOscCore::kAttack, 0.001}});
    c.noteOn(36, 1);
    const std::vector<float> y = pull(c, 8192);
    int nz = 0;
    for (float v : y) if (v != 0) nz++;
    row(nz > 0, "CONTROL level 0.001 must NOT be silent", "%.0f non-zero samples", (double)nz);
  }
  {
    auto tail = [](double flush) {
      SubOscCore c = make(44100, {{SubOscCore::kWave, SubOscCore::kSaw},
                                  {SubOscCore::kLevel, 1},
                                  {SubOscCore::kTone, 200},
                                  {SubOscCore::kAttack, 0.001},
                                  {SubOscCore::kRelease, 0.01}});
      c.flushFloor = flush;
      c.noteOn(36, 1);
      pull(c, (int)std::lround(0.2 * 44100));
      c.noteOff();
      const std::vector<float> y = pull(c, (int)std::lround(2.0 * 44100));
      long n = 0, last = 0;
      for (size_t i = 0; i < y.size(); i++)
      {
        const double a = std::fabs(y[i]);
        if (a != 0 && a < kF32MinNormal) n++;
        if (a != 0) last = (long)i;
      }
      std::printf("   flush %.0e: %ld subnormal sample(s); last non-zero at %ld of %zu\n",
                  flush, n, last, y.size());
      return n;
    };
    row(tail(SubOscCore::kFlush) == 0, "2.0 s of tail contains 0 float32-subnormal samples");
    // CONTROL: remove the flush-to-zero (the one test hook) and the tail decays
    // straight through the subnormal range. Limit L5: this counts subnormals in
    // the OUTPUT, the contract the host sees — it is not evidence about CPU
    // stalls, which this machine cannot show either way.
    row(tail(0) > 0, "CONTROL flush-to-zero removed must produce subnormals");
  }

  // ================================================ 6. §10.6 the bump's shape
  head("§10.6 bump: the two-lobe silhouette at the defaults, and a peak of exactly 1");
  {
    // DERIVED 2026-09-19 by rendering one period over an (a, phi) grid and
    // measuring the two positive lobes (traces/2026-09-19-b155-subosc-bump.md):
    // the trailing lobe is 0.8823 of the leading one (1.09 dB down — audible as
    // a shape, not as two events), the trough between them at 0.637 of the
    // leading peak. phi's SIGN decides the order.
    constexpr double kRatio = 0.8823, kTroughFrac = 0.6369, kTol = 0.05;
    bool ok = true;
    for (int m : {24, 36, 48, 60})
    {
      const Lobes r = bumpLobes(44100, m, 0.35, -0.25);
      if (r.pk.size() != 2)
      {
        std::printf("   MIDI %d: %zu positive lobes\n", m, r.pk.size());
        ok = false;
        continue;
      }
      const double ratio = r.pk[1] / r.pk[0];
      const double tf = r.tr.empty() ? std::nan("") : r.tr[0] / r.pk[0];
      std::printf("   MIDI %-3d f0 %7.2f Hz  period %7.1f smp  lobes %.5f / %.5f  "
                  "ratio %.5f  trough/peak %.5f  peak %.5f\n",
                  m, r.f0, r.per, r.pk[0], r.pk[1], ratio, tf, r.amax);
      if (std::fabs(ratio - kRatio) / kRatio > kTol) ok = false;
      if (!(std::fabs(tf - kTroughFrac) / kTroughFrac <= kTol)) ok = false;
    }
    row(ok, "bump lobe ratio = 0.8823 +-5 % at every note (big bump FIRST)");

    // CONTROL (must read ~1): at phi = 0 the lobes are exactly equal by
    // symmetry. If this still read 0.88 the measurement would be returning a
    // constant and the row above would be vacuous.
    const Lobes sym = bumpLobes(44100, 36, 0.35, 0);
    const double symRatio = sym.pk.size() == 2 ? sym.pk[1] / sym.pk[0] : 0;
    row(std::fabs(symRatio - 1) < 0.002, "CONTROL phi = 0 must give two EQUAL lobes (+-0.2 %)",
        "%.5f", symRatio);
    // CONTROL (must invert): phi = +0.25 is the mirror, so the SMALL bump
    // leads. This pins the human's ordering to the SIGN of the default rather
    // than to the indexing of the measurement.
    const Lobes mir = bumpLobes(44100, 36, 0.35, 0.25);
    const double mirRatio = mir.pk.size() == 2 ? mir.pk[1] / mir.pk[0] : 0;
    row(std::fabs(mirRatio - 1 / kRatio) / (1 / kRatio) < kTol,
        "CONTROL phi = +0.25 must INVERT the order (small bump leads)", "%.5f", mirRatio);
  }
  {
    // PEAK-EXACT, NOT MERELY BOUNDED (ADR-178 R7). The shape used to be divided
    // by the analytic bound 1 + a, provable in one operation but leaving the
    // bump at 0.750 of full scale at the defaults — up to 3.01 dB under every
    // other waveform (superseded limit L7). Swept, not spot-checked: the claim
    // is max|y| = 1 for EVERY (a, phi).
    double worstDev = 0, lo = 9, hi = 0, unnorm = 0;
    double atA = 0, atPhi = 0;
    for (double a : kAs)
      for (double phi : kPhis)
      {
        const double pk = shapePeak(a, phi);
        lo = std::min(lo, pk);
        hi = std::max(hi, pk);
        if (std::fabs(pk - 1) > worstDev) { worstDev = std::fabs(pk - 1); atA = a; atPhi = phi; }
        unnorm = std::max(unnorm, shapePeak(a, phi, 1.0));
      }
    std::printf("   8 x 9 grid, %d-point scan of one period: peak in [%.9f, %.9f], "
                "worst deviation %.2e (a=%.2f, phi=%.2f)\n",
                kPeakScan, lo, hi, worstDev, atA, atPhi);
    row(worstDev <= 1e-6, "bump peaks at 1.000000 +-1e-6 over the whole (a, phi) grid",
        "worst |peak - 1| = %.3e", worstDev);
    // CONTROL: the same scan with the normaliser removed must overshoot. Without
    // it, "peak <= 1" could be true because the shape is quiet rather than
    // because it is normalised.
    row(unnorm > 1.05, "CONTROL normaliser removed must overshoot 1", "worst %.6f", unnorm);
    // CONTROL: plant the OLD analytic bound back; the defaults must read 0.750,
    // the number §9's limit L7 recorded for it. A peak check that still read
    // 1.000 with the wrong normaliser in place would be measuring the scan and
    // not the core (L0032).
    const double bound = shapePeak(0.35, -0.25, 1 / (1 + 0.35));
    row(std::fabs(bound - 0.750) < 5e-4, "CONTROL old 1/(1+a) bound must read 0.750 at the defaults",
        "%.6f", bound);
  }

  // ================================ 7. full scale: no shape is quieter than the rest
  head("full scale: every shape reaches 1, which is the 3.01 dB ruling R7 bought");
  {
    // Measured through the WHOLE render path at the lowest note the pitch law
    // reaches (MIDI 0, octave -2, semis -12 = 1.03 Hz, 42 824 samples per
    // period at 44.1 kHz), so the render samples each shape densely enough for
    // the peak to be the SHAPE's and not the sampling grid's: the naive
    // triangle's apex is missed by at most 4 * (0.5/42824) = 4.7e-5, and that
    // resolution is the gate's floor rather than something tuned around.
    //
    // THE CEILING IS NOT GATED HERE, AND THE REASON IS MEASURED, NOT ASSERTED.
    // The discontinuous shapes read ~1.056 through the module, which is the TPT
    // one-pole's STEP OVERSHOOT at a near-Nyquist cutoff (its state pole sits at
    // 1-2G = -0.726 with the tone wide open), not the oscillator exceeding
    // unity: the same saw through a 200 Hz tone reads just UNDER 1, and that
    // pair of numbers is printed below. Limit L4 is the pin — "the numbers are
    // the MODULE's, not the oscillator's", because the tone filter is in the
    // signal path — and headroom below the module is the shell's business, not
    // the core's. The oscillator's own ceiling is gated where it can be seen
    // exactly: §10.6's 65 536-point shape scan, above.
    const char *names[] = {"sine", "triangle", "square", "saw", "pulse", "noise", "bump"};
    auto shapeRenderPeak = [](int w, double tone) {
      SubOscCore c = make(44100, {{SubOscCore::kWave, (double)w},
                                  {SubOscCore::kOctave, -2},
                                  {SubOscCore::kSemis, -12},
                                  {SubOscCore::kLevel, 1},
                                  {SubOscCore::kTone, tone},
                                  {SubOscCore::kWidth, 0.27},
                                  {SubOscCore::kAttack, 0.0005}});
      c.noteOn(0, 1);
      pull(c, 2048);  // past the attack ramp
      const std::vector<float> y = pull(c, 52000);
      double pk = 0;
      for (float v : y) pk = std::max(pk, (double)std::fabs(v));
      return pk;
    };
    double worstShort = 0;
    double pkSine = 0, pkBump = 0;
    for (int w = 0; w < SubOscCore::kWaveCount; w++)
    {
      const double pk = shapeRenderPeak(w, 20000);
      std::printf("   %-9s peak %.6f\n", names[w], pk);
      if (w == SubOscCore::kSine) pkSine = pk;
      if (w == SubOscCore::kBump) pkBump = pk;
      // Noise is printed but not gated on a peak: a full-band signal through a
      // near-Nyquist one-pole reads the FILTER's ringing (1.43), and the
      // shape's own amplitude claim — uniform on [-1, 1) — is pinned bit-for-bit
      // by the two seeded parity scenarios instead. A stated boundary, not a
      // silent exclusion (L0033).
      if (w != SubOscCore::kNoise) worstShort = std::max(worstShort, 1 - pk);
    }
    std::printf("   saw at tone 20000 = %.6f vs at tone 200 = %.6f — the excess above 1 is the "
                "TPT step overshoot, not the oscillator\n",
                shapeRenderPeak(SubOscCore::kSaw, 20000), shapeRenderPeak(SubOscCore::kSaw, 200));
    row(worstShort <= 1e-4,
        "the six deterministic shapes all reach full scale (within the render's 1e-4 resolution)",
        "worst shortfall %.2e", worstShort);
    // R7, end to end: the bump is as loud as the plainest shape through the
    // identical path. Its must-fail control is the shape-level row above — the
    // superseded 1/(1+a) bound reads 0.750, i.e. 3.01 dB under this.
    row(std::fabs(pkBump - pkSine) <= 1e-4,
        "bump reaches the SAME peak as the sine (R7; the old 1/(1+a) bound reads 0.750)",
        "|bump - sine| = %.2e", std::fabs(pkBump - pkSine));
  }

  // ====================== 8. the polyBLEP correction is present in THIS build
  head("§3/§10.1 the polyBLEP is in this build (Goertzel, not the lab's FFT — see the header)");
  {
    // The worst FOLDED harmonic of a saw at MIDI 60, read one bin at a time.
    // Harmonics above Nyquist fold to |k*f0 - round(k*f0/sr)*sr|; bins within
    // 25 Hz of a real harmonic are skipped so the fundamental's own skirt and
    // the true partials are never mistaken for an alias.
    const double sr = 44100;
    auto floorDb = [&](const std::vector<float> &y, double f0) {
      const double fund = goertzel(y, sr, f0);
      double worstMag = 0, worstF = 0;
      for (int k = (int)std::ceil(0.5 * sr / f0) + 1; k < 260; k++)
      {
        const double raw = k * f0;
        const double fold = std::fabs(raw - std::round(raw / sr) * sr);
        if (fold < 20 || fold > 0.5 * sr - 20) continue;
        bool nearHarmonic = false;
        for (int h = 1; h * f0 < 0.5 * sr; h++)
          if (std::fabs(fold - h * f0) < 25) { nearHarmonic = true; break; }
        if (nearHarmonic) continue;
        const double m = goertzel(y, sr, fold);
        if (m > worstMag) { worstMag = m; worstF = fold; }
      }
      std::printf("   worst folded bin %.0f Hz at %.1f dB below the fundamental\n",
                  worstF, 20 * std::log10(worstMag / fund));
      return 20 * std::log10(worstMag / fund);
    };
    SubOscCore c = make(sr, {{SubOscCore::kWave, SubOscCore::kSaw},
                             {SubOscCore::kOctave, 0},
                             {SubOscCore::kLevel, 1},
                             {SubOscCore::kTone, 20000},
                             {SubOscCore::kAttack, 0.0005}});
    c.noteOn(60, 1);
    pull(c, 4096);
    const std::vector<float> y = pull(c, 32768);
    const double f0 = c.freqHz();
    std::printf("   port   (polyBLEP): ");
    const double portDb = floorDb(y, f0);

    // CONTROL: the same note, same length, same detector, on a NAIVE saw — no
    // BLEP at all. §10.1 measured 11 dB of separation between the two at this
    // note with a Kaiser FFT; anything close to zero here would mean the
    // correction is not in the build (or that the detector cannot see it).
    std::vector<float> naive((size_t)y.size());
    {
      double ph = 0;
      const double d = f0 / sr;
      for (size_t i = 0; i < naive.size(); i++)
      {
        ph += d;
        ph -= std::floor(ph);
        naive[i] = (float)(2 * ph - 1);
      }
    }
    std::printf("   control (naive):   ");
    const double naiveDb = floorDb(naive, f0);
    row(portDb < naiveDb - 6, "polyBLEP buys real separation from the naive shape",
        "%.1f dB", naiveDb - portDb);
    row(naiveDb > -46, "CONTROL the naive saw must BREACH §10.1's MIDI-60 limit (-46 dB)",
        "%.1f dB", naiveDb);
  }

  // ========================================== 9. §7 the clamped table contract
  head("§7/D2 one clamped table: setParam clamps every known key and throws on an unknown one");
  {
    SubOscCore c(48000);
    bool threw = false;
    try { c.setParam("notAParameter", 1); }
    catch (const std::exception &) { threw = true; }
    row(threw, "setParam throws on an unknown address (audit A2: no silent no-op)");

    bool threwKnown = false;
    try { c.setParam("wave", 999); }
    catch (const std::exception &) { threwKnown = true; }
    // CONTROL: a KNOWN key must not throw — a setParam that rejected everything
    // would pass the row above while being useless.
    row(!threwKnown, "CONTROL a known address does not throw");
    row(c.param(SubOscCore::kWave) == SubOscCore::kWaveCount - 1,
        "an out-of-range value lands on the bound, it does not walk past it",
        "wave = %.0f", c.param(SubOscCore::kWave));
    c.setParam("octave", 7);
    c.setParam("tone", -1000);
    c.setParam("bumpAmt", 99);
    row(c.param(SubOscCore::kOctave) == 0 && c.param(SubOscCore::kTone) == 30 &&
            c.param(SubOscCore::kBumpAmt) == 0.6,
        "octave / tone / bumpAmt clamp to §7's bounds from both directions");
    // JS Math.round is half-UP, not half-away-from-zero: octave -1.5 is -1 in
    // the lab and would be -2 under std::round. A stepped parameter that lands
    // one step away from the lab's is a parity failure nothing else here sees,
    // because no golden happens to set a stepped parameter to a half-integer.
    c.setParam("octave", -1.5);
    row(c.param(SubOscCore::kOctave) == -1, "stepped parameters round half-UP, as JS does",
        "octave(-1.5) = %.0f", c.param(SubOscCore::kOctave));
  }

  // ==================================================================== 10. CPU
  {
    // REPORTED, NEVER GATED — the absolute number is machine-dependent.
    // SPEC-SUBOSC §10.7 lists "CPU per voice" as still to measure before a
    // port; this is that measurement, taken on the standalone core. 16
    // INSTANCES, because §1/§8 make the sub a PER-VOICE source: 16 voices of
    // the device means 16 of these, not one rendering 16 notes.
    const int secs = 5, N = 48000 * secs;
    double best = 1e30;
    for (int t = 0; t < 3; t++)
    {
      std::vector<SubOscCore> v;
      v.reserve(16);
      for (int i = 0; i < 16; i++)
      {
        v.push_back(make(48000, {{SubOscCore::kWave, SubOscCore::kPulse},
                                 {SubOscCore::kWidth, 0.27},
                                 {SubOscCore::kTone, 1200},
                                 {SubOscCore::kSync, 1}}));
        v.back().noteOn(36 + i, 1);
      }
      const std::vector<float> master = masterPhase(110, 48000, 256);
      std::vector<float> L(256), R(256);
      const auto t0 = std::chrono::steady_clock::now();
      for (int off = 0; off < N; off += 256)
      {
        const int k = std::min(256, N - off);
        for (auto &c : v) c.render(L.data(), R.data(), k, master.data());
      }
      best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    }
    const double pct = best / secs * 100;
    std::printf("\n-- CPU (REPORT, not gated) --\n");
    std::printf("   16 instances (16 voices' worth), pulse + tone + hard sync, 48 kHz, 5 s, "
                "min of 3: %.4f s  =  %.3f %% of one core\n", best, pct);
    std::printf("   SPEC-SUBOSC §10.7 claims no budget; this is the first measurement. It is "
                "load-sensitive — treat a single reading as a sample, not as the figure.\n");
  }

  std::printf("\nsubosc_check: %s (%d failure%s; worst parity rms %.3e)\n",
              g_failures ? "RED" : "GREEN", g_failures, g_failures == 1 ? "" : "s", worst);
  return g_failures ? 1 : 0;
}
