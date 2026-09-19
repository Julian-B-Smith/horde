/*
 * station_check — the STATION port's oracle (B153 layer 3, phase 1).
 *
 * WHAT IT GATES
 *  1. L0-1 parity: `hypersaw::StationCore` reproduces reference/station.html's
 *     `StationCore` to <= 1e-6 RMS on every scenario, at BOTH 48 000 and
 *     44 100 Hz. Goldens from tools/golden/gen_station_goldens.mjs, which
 *     slices the lab live and inverts its monitoring tanh exactly (see that
 *     file's header) so parity is measured at full engine amplitude.
 *  2. The truncated PM literal `0.1591549` is LOAD-BEARING — proved, not
 *     asserted: the row re-renders with the true 1/(2pi) and reports the RMS it
 *     fails at, then restores. (audit S3 measured 2.156e-6 at index 8.)
 *  3. The behavioural rows SPEC-STATION §11/§12 state parity structurally
 *     cannot see: LFSR periods, noise decorrelation, the Nyquist mute,
 *     block-size and sample-rate independence, silence, denormals, feedback
 *     stability, the one-sample delay, DC — and the five deliberate
 *     divergences, each with the control that must read the other answer.
 *  4. A CPU number against SPEC §12's "<= ~2 % of one core at 16 voices".
 *     REPORTED, never gated: the absolute ratio is machine-dependent.
 *
 * WIRED in `./verify full` (human ruling 2026-09-19, ADR-179 §4 — the wiring
 * default inverted), beside swarmalator's chain: generator --selfcheck,
 * generator, then this binary with the golden dir as argv[1].
 *
 * EVERY ROW CARRIES A CONTROL that must read the other answer (L0016/L0032).
 * Where the lab harness plants a defective build to get its must-fail control
 * (tools/labharness/station_check.mjs), C++ cannot; the equivalent here is a
 * row that asserts BOTH directions — the quantity under test AND a
 * deliberately-wrong hypothesis that the same detector must reject.
 *
 * TWO ROWS THE AUDIT ASKED FOR THAT ARE NOT HERE, AND WHY:
 *  · The Bessel sideband SIGN pattern (audit §4.4) is NOT a detector for the
 *    one-sample delay and is deliberately not gated. labharness/station_check.mjs
 *    S8 planted a no-delay build and it reproduced the pattern to 3 decimals:
 *    one sample of delay on a sinusoidal modulator is a pure phase rotation and
 *    |Jn| is invariant under it. This file uses the LAG RESIDUAL instead, which
 *    the same experiment showed does separate the two hypotheses by 5 orders.
 *  · The DRW pure branch's ALIAS FLOOR is gated, but by GOERTZEL at the folded
 *    bins rather than by the 65 536-point windowed FFT the audit used: no FFT
 *    harness exists on the C++ side and one bin at a time is all this row needs.
 *    The number is therefore comparable to, not identical with, audit §2.1's.
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../src/station_core.h"

using hypersaw::StationCore;

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

constexpr double kTau = 6.283185307179586;
double noteFreq(int m) { return 440 * std::pow(2, (m - 69) / 12.0); }

double rms(const std::vector<double> &a)
{
  double s = 0;
  for (double v : a) s += v * v;
  return std::sqrt(s / std::max<size_t>(1, a.size()));
}
double peak(const std::vector<double> &a)
{
  double m = 0;
  for (double v : a) m = std::max(m, std::fabs(v));
  return m;
}
double mean(const std::vector<double> &a)
{
  double s = 0;
  for (double v : a) s += v;
  return s / std::max<size_t>(1, a.size());
}

// ---------------------------------------------------------------- scenarios --
// A manifest row: the WHOLE lab state as flat key=value tokens plus @meta. The
// C++ mirrors nothing — see the generator's header on why a mirrored scenario
// table in two languages is a silent-drift machine.
struct Scenario
{
  std::string name;
  double sr = 48000, secs = 1;
  int block = 128, offAt = -1, dcblock = 1;
  std::vector<std::pair<int, double>> notes;
  std::vector<std::pair<std::string, double>> keys;
};

// Applies one dumped key to the core's Patch. An UNKNOWN KEY IS A FAILURE, not
// a shrug: it means the lab grew a field this port does not carry, which is
// exactly the silent parity hole the dump exists to prevent.
bool applyKey(StationCore &c, const std::string &k, double v, uint8_t *tbl)
{
  auto &P = c.patch;
  if (k.rfind("ops.", 0) == 0)
  {
    const int i = k[4] - '0';
    if (i < 0 || i >= StationCore::kOps) return false;
    const std::string f = k.substr(6);
    auto &o = P.ops[i];
    if (f == "on") o.on = (int)v;
    else if (f == "wave") o.wave = (int)v;
    else if (f == "mode") o.mode = (int)v;
    else if (f == "coarse") o.coarse = v;
    else if (f == "fine") o.fine = v;
    else if (f == "semis") o.semis = v;
    else if (f == "fixed") o.fixed = v;
    else if (f == "lvl") o.lvl = v;
    else if (f == "pan") o.pan = v;
    else if (f == "pw") o.pw = v;
    else if (f == "pure") o.pure = v;
    else if (f == "qnt") o.qnt = (int)v;
    else if (f == "sync") o.sync = (int)v;
    else if (f == "env.a") o.env.a = v;
    else if (f == "env.d") o.env.d = v;
    else if (f == "env.s") o.env.s = v;
    else if (f == "env.r") o.env.r = v;
    else if (f == "env.loop") o.env.loop = (int)v;
    else return false;
    return true;
  }
  if (k.rfind("noise.", 0) == 0)
  {
    const std::string f = k.substr(6);
    auto &n = P.noise;
    if (f == "on") n.on = (int)v;
    else if (f == "mode") n.mode = (int)v;
    else if (f == "rate") n.rate = v;
    else if (f == "ktrk") n.ktrk = (int)v;
    else if (f == "lvl") n.lvl = v;
    else if (f == "pan") n.pan = v;
    else if (f == "env.a") n.env.a = v;
    else if (f == "env.d") n.env.d = v;
    else if (f == "env.s") n.env.s = v;
    else if (f == "env.r") n.env.r = v;
    else if (f == "env.loop") n.env.loop = (int)v;
    else return false;
    return true;
  }
  if (k.rfind("matrix.", 0) == 0)
  {
    const int r = k[7] - '0', c2 = k[9] - '0';
    if (r < 0 || r >= StationCore::kSlots || c2 < 0 || c2 >= StationCore::kOps) return false;
    P.matrix[r][c2] = v;
    return true;
  }
  if (k == "pitchEnv.amt") { P.pitchEnv.amt = v; return true; }
  if (k == "pitchEnv.dec") { P.pitchEnv.dec = v; return true; }
  if (k.rfind("table.", 0) == 0)
  {
    const int i = std::atoi(k.c_str() + 6);
    if (i < 0 || i >= StationCore::kTable) return false;
    tbl[i] = (uint8_t)v;
    return true;
  }
  if (k == "seed") { P.seed = (uint32_t)v; return true; }
  return false;
}

// Builds the core from a scenario and renders it, returning interleaved stereo.
std::vector<double> renderScenario(const Scenario &sc, bool *keyOk = nullptr)
{
  StationCore c(sc.sr);
  c.dcBlock = sc.dcblock != 0;
  uint8_t tbl[StationCore::kTable];
  std::memcpy(tbl, c.patch.table, sizeof(tbl));
  bool ok = true;
  for (const auto &kv : sc.keys)
    if (!applyKey(c, kv.first, kv.second, tbl))
    {
      ok = false;
      std::printf("     unknown manifest key '%s'\n", kv.first.c_str());
    }
  c.setTable(tbl);
  if (keyOk) *keyOk = ok;
  for (const auto &n : sc.notes) c.noteOn(n.first, n.second);

  const int total = (int)std::lround(sc.sr * sc.secs);
  std::vector<double> out((size_t)total * 2);
  std::vector<float> L((size_t)sc.block), R((size_t)sc.block);
  for (int off = 0; off < total;)
  {
    int k = std::min(sc.block, total - off);
    if (sc.offAt > off && sc.offAt < off + k) k = sc.offAt - off;
    if (off == sc.offAt)
      for (const auto &n : sc.notes) c.noteOff(n.first);
    c.render(L.data(), R.data(), k);
    for (int i = 0; i < k; i++)
    {
      out[(size_t)(off + i) * 2] = L[i];
      out[(size_t)(off + i) * 2 + 1] = R[i];
    }
    off += k;
  }
  return out;
}

// ------------------------------------------------------------ probe helpers --
// A blank core: silent operators, empty matrix, no noise — every probe starts
// here and switches on exactly the one thing it measures (the labharness
// `blank()` idiom, so the two suites' numbers are comparable).
StationCore blank(double sr = 48000)
{
  StationCore c(sr);
  auto &s = c.patch;
  for (int r = 0; r < StationCore::kSlots; r++)
    for (int k = 0; k < StationCore::kOps; k++) s.matrix[r][k] = 0;
  for (auto &o : s.ops)
  {
    o.on = 1;
    o.lvl = 0;
    o.pan = 0;
    o.qnt = 0;
    o.pure = 1;
    o.sync = 0;
    o.env = StationCore::Env{1, 10, 1, 60, 0, 0};  // steady state in ~11 ms
  }
  s.noise.on = 0;
  s.pitchEnv.amt = 0;
  return c;
}

std::vector<double> pull(StationCore &c, int n, int bs = 0)
{
  std::vector<double> out((size_t)n);
  const int b = bs ? bs : n;
  std::vector<float> L((size_t)b), R((size_t)b);
  for (int off = 0; off < n;)
  {
    const int k = std::min(b, n - off);
    c.render(L.data(), R.data(), k);
    for (int i = 0; i < k; i++) out[(size_t)(off + i)] = L[i];
    off += k;
  }
  return out;
}

// The audit §1.4 max patch: every cell at 8, DRW at PURE 0.5 / QNT 16, looping
// envelopes, SHORT noise with KEYTRK, +24 st pitch env. Worst case for every
// code path at once — used by the block-size, silence, denormal and CPU rows.
StationCore maxPatch(double sr = 48000)
{
  StationCore c(sr);
  auto &s = c.patch;
  for (int r = 0; r < StationCore::kSlots; r++)
    for (int k = 0; k < StationCore::kOps; k++) s.matrix[r][k] = 8;
  for (auto &o : s.ops)
  {
    o.on = 1;
    o.wave = StationCore::kDrw;
    o.lvl = 0.8;
    o.pure = 0.5;
    o.qnt = 16;
    o.pw = 0.37;
    o.env = StationCore::Env{3, 420, 0.55, 260, 1, 0};
  }
  s.noise = StationCore::Noise{1, 1, 0.8, 1, 0.7, 0, StationCore::Env{1, 120, 0.6, 80, 0, 0}};
  s.pitchEnv.amt = 24;
  s.pitchEnv.dec = 800;
  s.seed = 1024;
  c.loadFactoryTable(StationCore::kRnd);
  return c;
}

// Fundamental frequency by interpolated positive-going zero crossings. Used
// instead of an FFT peak-bin read for the Nyquist row: the signal there is a
// pure sine by construction, and this resolves far finer than one FFT bin.
double zcFreq(const std::vector<double> &x, double sr)
{
  int n = 0;
  double first = -1, last = -1;
  for (size_t i = 1; i < x.size(); i++)
    if (x[i - 1] <= 0 && x[i] > 0)
    {
      const double t = (i - 1) + (0 - x[i - 1]) / (x[i] - x[i - 1]);
      if (first < 0) first = t;
      last = t;
      n++;
    }
  if (n < 2) return 0;
  return (n - 1) * sr / (last - first);
}

// Goertzel: the magnitude of ONE frequency, which is all the alias row needs
// and is ~12 lines against a 65 536-point FFT harness that does not exist on
// the C++ side. A Hann window keeps the skirt of the (enormous) fundamental off
// the (tiny) alias bins being read.
double goertzel(const std::vector<double> &x, double sr, double f)
{
  const double w = kTau * f / sr;
  const double c = 2 * std::cos(w);
  double s1 = 0, s2 = 0;
  const size_t N = x.size();
  for (size_t i = 0; i < N; i++)
  {
    const double win = 0.5 - 0.5 * std::cos(kTau * (double)i / (double)(N - 1));
    const double s0 = x[i] * win + c * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) * 2 / (double)N;
}

// Mean squared second difference, normalised by the signal's own power: a
// cheap, FFT-free "how much high-frequency energy is in here" statistic.
double roughness(const std::vector<double> &x)
{
  double num = 0, den = 0;
  for (size_t i = 2; i < x.size(); i++)
  {
    const double d = x[i] - 2 * x[i - 1] + x[i - 2];
    num += d * d;
    den += x[i] * x[i];
  }
  return den > 0 ? num / den : 0;
}
}  // namespace

int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/station";

  // ================================================================ 1. parity
  std::ifstream mf(dir + "/station-manifest.tsv");
  if (!mf)
  {
    std::printf("FAIL station manifest missing (run tools/golden/gen_station_goldens.mjs)\n");
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
      else if (k == "@dcblock") sc.dcblock = std::atoi(v.c_str());
      else if (k == "@note")
      {
        const auto col = v.find(':');
        sc.notes.push_back({std::atoi(v.c_str()), std::atof(v.c_str() + col + 1)});
      }
      else sc.keys.push_back({k, std::atof(v.c_str())});
    }
    scenarios.push_back(sc);
  }
  std::printf("-- parity (L0-1, eps = 1e-6 RMS) -- %zu scenarios, lab DC blocker %s --\n",
              scenarios.size(), scenarios.empty() || scenarios[0].dcblock ? "PRESENT" : "ABSENT");

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
    const std::vector<double> got = renderScenario(sc, &keyOk);
    double acc = 0, era = 0;
    const size_t n = std::min(ref.size(), got.size());
    for (size_t i = 0; i < n; i++)
    {
      const double d = got[i] - ref[i];
      acc += d * d;
      era += (double)ref[i] * ref[i];
    }
    const double r = std::sqrt(acc / std::max<size_t>(1, n)), refRms = std::sqrt(era / std::max<size_t>(1, n));
    worst = std::max(worst, r);
    char buf[192];
    std::snprintf(buf, sizeof(buf), "parity %-16s (ref rms %.4f, %zu samples)  rms =",
                  sc.name.c_str(), refRms, n);
    row(keyOk && ref.size() == got.size() && r <= 1e-6 && refRms > 1e-4, buf, "%.3e", r);
  }
  std::printf("   worst parity rms over all scenarios: %.3e  (gate 1e-6)\n", worst);

  // ============================================ 2. the truncated PM constant
  // audit S3: 1/(2pi) written to full double is a relative error of 2.708e-7 and
  // produces an RMS difference of 2.156e-6 at cell index 8 -- 2.2x OVER eps.
  // THE MEASUREMENT IS CORE-vs-CORE, NOT CORE-vs-GOLDEN, and deliberately so:
  // what has to be proved is that the constant MOVES THE OUTPUT BY MORE THAN THE
  // BUDGET, and that is a property of the engine at the cell maximum, which no
  // parity scenario can carry (index 8 on the diagonal is chaotic -- see the
  // generator's `selffb` note). The audit's own patch: SAW carrier, sine
  // modulator at ratio 7:1, 2 s.
  {
    auto renderPm = [](double pm, double index) {
      StationCore c = blank();
      c.dcBlock = false;
      c.pmConst = pm;
      c.patch.ops[0].wave = StationCore::kSin;
      c.patch.ops[0].coarse = 7;
      c.patch.ops[0].lvl = 0;
      c.patch.ops[1].wave = StationCore::kSaw;
      c.patch.ops[1].coarse = 1;
      c.patch.ops[1].lvl = 1;
      c.patch.ops[2].on = 0;
      c.patch.matrix[0][1] = index;
      c.noteOn(36, noteFreq(36));
      return pull(c, 96000);
    };
    std::printf("-- the truncated PM literal (audit S3): SAW carrier / sine modulator 7:1, 2 s --\n");
    bool overBudget = false;
    for (double index : {1.0, 2.6, 4.0, 8.0})
    {
      const std::vector<double> a = renderPm(StationCore::kPmConst, index);
      const std::vector<double> b = renderPm(1.0 / kTau, index);
      double acc = 0, mx = 0;
      for (size_t i = 0; i < a.size(); i++)
      {
        const double d = a[i] - b[i];
        acc += d * d;
        mx = std::max(mx, std::fabs(d));
      }
      const double r = std::sqrt(acc / (double)a.size());
      std::printf("   index %-4.1f  0.1591549 vs 1/(2pi):  rms %.4e   max %.4e   %s eps = 1e-6\n",
                  index, r, mx, r > 1e-6 ? "OVER " : "under");
      if (index == 8.0 && r > 1e-6) overBudget = true;
    }
    row(overBudget, "the truncated PM literal is LOAD-BEARING: 1/(2pi) exceeds eps at index 8", nullptr, 0);
  }

  // ============================================================= 3. behavioural
  std::printf("-- behavioural rows (what parity structurally cannot see) --\n");

  // 3.1 LFSR periods. SPEC §6 names both numbers; they are the identity of the
  // NES noise channel. The CONTROL is a wrong tap, which must NOT give 32767.
  {
    auto walk = [](uint32_t seed, int tapBit) {
      uint32_t l = seed;
      int i = 0;
      for (; i < 200000; i++)
      {
        const uint32_t b0 = l & 1u, tap = (l >> tapBit) & 1u;
        l = (l >> 1) | (((b0 ^ tap) & 1u) << 14);
        if (l == seed) return i + 1;
      }
      return -1;
    };
    StationCore c = blank();
    c.patch.noise.on = 1;
    c.patch.noise.lvl = 0;
    c.noteOn(60, noteFreq(60));
    const uint32_t s0 = c.voiceAt(0).lfsr;
    const int lng = walk(s0, 1), shrt = walk(s0, 6), bad = walk(s0, 2);
    std::printf("   voice seed 0x%04x  LONG period %d  SHORT period %d  (spec: 32767 / 93)\n",
                s0, lng, shrt);
    row(s0 != 0 && s0 <= 0x7FFF && lng == 32767 && shrt == 93 && bad != 32767,
        "LFSR: LONG 32767, SHORT 93, seed nonzero+15-bit; control tap bit2",
        "control period %.0f (must differ)", (double)bad);
  }

  // 3.2 Noise decorrelation. SPEC §11.7 gates the RULE, not the derivation:
  // N independent streams sum as sqrt(N), N COHERENT streams sum as N. The
  // control forces every voice onto one seed and must read ~N.
  {
    auto run = [](int nv, bool forceOneSeed) {
      StationCore c = blank();
      for (auto &o : c.patch.ops) { o.on = 0; o.lvl = 0; }
      c.patch.noise = StationCore::Noise{1, 0, 0.35, 0, 1, 0, StationCore::Env{1, 120, 1, 80, 0, 0}};
      for (int i = 0; i < nv; i++) c.noteOn(60, noteFreq(60));
      if (forceOneSeed)
        for (int i = 1; i < nv; i++) c.voiceAt(i).lfsr = c.voiceAt(0).lfsr;
      return rms(pull(c, 48000));
    };
    const double r1 = run(1, false), r4 = run(4, false), ratio = r4 / r1;
    const double c1 = run(1, true), c4 = run(4, true), cratio = c4 / c1;
    std::printf("   independent 1v %.4e  4v %.4e  ratio %.3f  (sqrt(4) = 2.000)\n", r1, r4, ratio);
    row(ratio <= 2.2 && cratio >= 3.5,
        "noise decorrelates as sqrt(N); control: one seed for all voices reads ~N",
        "control ratio %.3f (coherent = 4.000)", cratio);
  }

  // 3.3 The Nyquist mute (ADR-177 §3). An operator above Nyquist is SILENT, never
  // detuned. The pre-fix clamp min(f/SR, 0.45) made the top 1.5 octaves' pitch
  // depend on the host sample rate with the SHIPPED patch. Tolerance in Hz, not
  // percent — a percentage gate at MIDI 24 fails the detector's own resolution.
  {
    int wrong = 0, silentBelow = 0, mutedAbove = 0, n = 0;
    double worstHz = 0;
    for (double sr : {44100.0, 48000.0, 96000.0})
      for (double ratio : {1.0, 7.0, 14.0})
        for (int midi = 24; midi <= 108; midi += 6)
        {
          StationCore c = blank(sr);
          for (int i = 0; i < 3; i++) { c.patch.ops[i].on = (i == 2); c.patch.ops[i].lvl = (i == 2) ? 0.85 : 0; }
          c.patch.ops[2].coarse = ratio;
          c.noteOn(midi, noteFreq(midi));
          const std::vector<double> y = pull(c, (int)sr);
          const double want = noteFreq(midi) * ratio;
          n++;
          if (peak(y) == 0)
          {
            if (want < sr * 0.5) { silentBelow++; wrong++; }
            else mutedAbove++;
            continue;
          }
          if (want >= sr * 0.5) { wrong++; continue; }  // sounding above Nyquist is wrong too
          const double got = zcFreq(y, sr);
          worstHz = std::max(worstHz, std::fabs(got - want));
          if (std::fabs(got - want) > std::max(0.5, want * 1e-4)) wrong++;
        }
    std::printf("   %d (rate x ratio x note) cases: wrong-pitch %d, silent-below-Nyquist %d, correctly muted above %d\n",
                n, wrong, silentBelow, mutedAbove);
    row(wrong == 0 && mutedAbove > 0,
        "Nyquist: correct pitch below, silent above, MIDI 24..108 x {1,7,14} x 3 rates",
        "worst deviation where it sounds %.3f Hz", worstHz);
  }

  // 3.4 Block-size independence. A host handing 333 frames must get exactly what
  // a host handing 128 gets. The control is the detector's own resolution: it
  // must be able to see a difference at all, so a deliberately altered patch
  // must read non-zero through the same comparison.
  {
    auto renderMax = [](int bs) {
      StationCore c = maxPatch();
      c.noteOn(60, noteFreq(60));
      c.noteOn(67, noteFreq(67));
      return pull(c, 24000, bs);
    };
    const std::vector<double> ref = renderMax(128);
    double d = 0;
    for (int bs : {1, 7, 333, 4096})
    {
      const std::vector<double> got = renderMax(bs);
      for (size_t i = 0; i < ref.size(); i++) d = std::max(d, std::fabs(ref[i] - got[i]));
    }
    StationCore c2 = maxPatch();
    c2.patch.ops[0].coarse = 1.0000001;
    c2.noteOn(60, noteFreq(60));
    c2.noteOn(67, noteFreq(67));
    const std::vector<double> alt = pull(c2, 24000, 128);
    double dc = 0;
    for (size_t i = 0; i < ref.size(); i++) dc = std::max(dc, std::fabs(ref[i] - alt[i]));
    row(d == 0 && dc > 0, "block size {1,7,128,333,4096} bit-identical; control: a 1e-7 ratio nudge is visible",
        "max|diff| %.1e", d);
    std::printf("   control (ratio nudged by 1e-7) max|diff| %.3e  (gate: > 0)\n", dc);
  }

  // 3.5 Sample-rate portability of the envelopes (ADR-009's trap). The audit
  // measured attack 1.587 % / decay 0.154 % on the lab; the port inherits both
  // because the Euler form is copied. Pinned, not wished away.
  {
    auto landmarks = [](double sr) {
      StationCore c = blank(sr);
      for (int i = 0; i < 3; i++)
      {
        c.patch.ops[i].on = (i == 0);
        c.patch.ops[i].lvl = (i == 0) ? 1 : 0;
        c.patch.ops[i].env = StationCore::Env{50, 400, 0.3, 200, 0, 0};
      }
      c.noteOn(60, noteFreq(60));
      double atk = -1, dec = -1;
      float L, R;
      for (int n = 0; n < (int)(sr * 2); n++)
      {
        c.render(&L, &R, 1);
        if (atk < 0 && c.voiceAt(0).env[0].stage >= 1) atk = n / sr;
        if (dec < 0 && c.voiceAt(0).env[0].stage >= 2) { dec = n / sr; break; }
      }
      return std::pair<double, double>{atk, dec};
    };
    const auto a = landmarks(44100), b = landmarks(48000), c3 = landmarks(96000);
    const double atkSpread = (std::max({a.first, b.first, c3.first}) - std::min({a.first, b.first, c3.first})) / a.first * 100;
    const double decSpread = (std::max({a.second, b.second, c3.second}) - std::min({a.second, b.second, c3.second})) / a.second * 100;
    std::printf("   attack complete  44.1k %.5fs  48k %.5fs  96k %.5fs   drift %.3f %%  (gate <= 1.6)\n",
                a.first, b.first, c3.first, atkSpread);
    std::printf("   decay  complete  44.1k %.5fs  48k %.5fs  96k %.5fs   drift %.3f %%  (gate <= 0.5)\n",
                a.second, b.second, c3.second, decSpread);
    row(atkSpread <= 1.6 && decSpread <= 0.5 && atkSpread > 0,
        "envelope landmarks are sample-rate portable (the lab's own bound)",
        "attack drift %.3f %%", atkSpread);
  }

  // 3.6 Silence in -> silence out. Exactly 0, not "small": the shell mixes 16.
  // The control is one gated voice, which must be non-zero throughout — a row
  // that passes because the engine is broken certifies nothing.
  {
    StationCore c = maxPatch();
    const std::vector<double> q = pull(c, 96000, 256);
    size_t nz = 0;
    for (double v : q) nz += (v != 0);
    StationCore c2 = maxPatch();
    c2.noteOn(60, noteFreq(60));
    const std::vector<double> g = pull(c2, 96000, 256);
    size_t nz2 = 0;
    for (double v : g) nz2 += (v != 0);
    std::printf("   control  one gated voice -> %zu non-zero samples of 96000  (gate > 90000)\n", nz2);
    row(nz == 0 && nz2 > 90000, "no voices -> exactly 0 for 2 s", "non-zero samples %.0f", (double)nz);
  }

  // 3.7 Denormals. Structural here (the envelope's absolute exits snap to zero
  // long before the subnormal range), so this row is a watch on that structure.
  // The control proves the detector can see a subnormal at all.
  {
    StationCore c = maxPatch();
    c.noteOn(60, noteFreq(60));
    c.noteOn(67, noteFreq(67));
    pull(c, 48000, 256);
    c.noteOff(60);
    c.noteOff(67);
    const std::vector<double> t = pull(c, 48000 * 6, 256);
    size_t sub = 0;
    for (double v : t)
    {
      const float f = (float)v;
      if (f != 0 && std::fabs(f) < 1.1754943508222875e-38f) sub++;
    }
    int probe = 0;
    for (float f : {1e-40f, 5e-39f, 0.0f, 1e-20f})
      if (f != 0 && std::fabs(f) < 1.1754943508222875e-38f) probe++;
    std::printf("   control  detector on [1e-40, 5e-39, 0, 1e-20] finds %d  (gate: exactly 2)\n", probe);
    row(sub == 0 && probe == 2, "no subnormal output samples, 6 s past note-off on the max patch",
        "subnormals %.0f", (double)sub);
  }

  // 3.8 Self-feedback stability at the cell maximum. SPEC §2's one-sample delay
  // is what makes this unconditionally stable. The control is index 0, which
  // must be a plain sine — a stability probe that cannot tell chaos from a sine
  // is useless.
  {
    bool ok = true;
    double worstOp = 0;
    int nonFinite = 0;
    for (int d = 0; d < 3; d++)
    {
      StationCore c = blank();
      // The blocker is BYPASSED here on purpose: this row measures the
      // OPERATOR's bound, and a 5 Hz highpass's settling transient overshoots
      // the engine sum at note-on, which would be read as instability in the
      // thing this row is not measuring.
      c.dcBlock = false;
      for (int i = 0; i < 3; i++) { c.patch.ops[i].on = (i == d); c.patch.ops[i].lvl = (i == d) ? 1 : 0; }
      c.patch.matrix[d][d] = 8;
      c.noteOn(60, noteFreq(60));
      const std::vector<double> y = pull(c, 48000 * 10, 512);
      double mx = 0;
      for (double v : y)
      {
        if (!std::isfinite(v)) nonFinite++;
        mx = std::max(mx, std::fabs(v));
      }
      const double opPeak = mx / 0.35;  // the per-slot mix gain; lvl = 1, no master
      worstOp = std::max(worstOp, opPeak);
      ok = ok && opPeak <= 1 + 1e-6;
    }
    StationCore c0 = blank();
    c0.dcBlock = false;
    for (int i = 0; i < 3; i++) { c0.patch.ops[i].on = (i == 0); c0.patch.ops[i].lvl = (i == 0) ? 1 : 0; }
    c0.noteOn(60, noteFreq(60));
    const std::vector<double> sine = pull(c0, 48000);
    const double rough0 = roughness(sine);
    std::printf("   control  index 0 is a plain sine: roughness %.3e  (gate < 1e-3)\n", rough0);
    row(ok && nonFinite == 0 && rough0 < 1e-3,
        "self-feedback index 8 bounded + finite over 10 s, all 3 diagonals",
        "worst |op| %.6f (gate <= 1.000001)", worstOp);
  }

  // 3.9 The one-sample delay (SPEC §2). NOT the audit's sideband-sign detector,
  // which labharness S8 disproved. This reconstructs the operator's own defining
  // equation at lag 1 and at lag 0 and asserts the residual separates them: a
  // port reading the CURRENT sample would come out the other way round, so the
  // lag-0 assertion IS the must-fail control in C++ form.
  {
    const double I = 2;
    StationCore c = blank();
    c.dcBlock = false;  // the model fitted below is the OPERATOR's, not the output stage's
    c.patch.ops[0].wave = StationCore::kSin;
    c.patch.ops[0].coarse = 7;
    c.patch.ops[0].lvl = 0;  // OP1 = modulator
    c.patch.ops[1].wave = StationCore::kSin;
    c.patch.ops[1].coarse = 1;
    c.patch.ops[1].lvl = 1;  // OP2 = carrier
    c.patch.ops[2].on = 0;
    c.patch.matrix[0][1] = I;
    c.noteOn(36, noteFreq(36));
    float L, R;
    for (int n = 0; n < 2000; n++) c.render(&L, &R, 1);  // settle past the attack
    const int N = 4000;
    std::vector<double> ph(N), m(N), y(N);
    for (int n = 0; n < N; n++)
    {
      c.render(&L, &R, 1);
      ph[n] = c.voiceAt(0).ph[1];
      m[n] = c.voiceAt(0).prev[0];
      y[n] = L / 0.35;  // undo the slot mix gain; lvl = 1, pan 0, no master
    }
    auto residual = [&](int lag) {
      double e = 0;
      for (int n = 1; n < N; n++)
      {
        double p = ph[n] + I * m[n - lag] * StationCore::kPmConst;
        p -= std::floor(p);
        e = std::max(e, std::fabs(y[n] - std::sin(p * kTau)));
      }
      return e;
    };
    const double l1 = residual(1), l0 = residual(0);
    std::printf("   max|out - model(lag 1)| %.3e      max|out - model(lag 0)| %.3e\n", l1, l0);
    row(l1 < 1e-6 && l0 > 0.01,
        "matrix taps read the PREVIOUS sample; the lag-0 hypothesis is rejected",
        "lag0/lag1 = %.0fx", l0 / std::max(l1, 1e-300));
  }

  // 3.10 DC. The human's 2026-09-19 ruling put a one-pole blocker on the engine
  // output; labharness S17 PINS the pre-blocker numbers (PLS pw 0.1 at -1.9 dB
  // below peak, SHORT noise -29.9, self-feedback -26.7). The bypass is the
  // control: it must reproduce those, or the blocker row is passing because the
  // probe cannot see DC at all.
  {
    // `pinned` separates the two kinds of row. S17's SIN and QTR entries are
    // CONTROLS -- they have no DC, so their number is whatever analysis floor
    // the window length produces, and asserting S17's -61.2/-64.5 here would be
    // comparing two floors measured over different windows (this probe settles
    // for 1 s first, S17 does not). They are gated as "nothing measurable".
    // The three rows that DO carry DC are pinned to S17 and reproduce it to
    // within 1 dB, which is what makes the bypass leg a real control.
    struct Cfg { const char *name; double pin; bool pinned; void (*mut)(StationCore &); };
    const Cfg cfgs[] = {
      {"SIN, no feedback", -60, false, [](StationCore &c) { c.patch.ops[0].wave = 0; c.patch.ops[0].lvl = 1; }},
      {"QTR raw", -60, false, [](StationCore &c) { c.patch.ops[0].wave = 4; c.patch.ops[0].lvl = 1; c.patch.ops[0].pure = 0; }},
      {"self-feedback idx 8", -26.7, true, [](StationCore &c) { c.patch.ops[0].wave = 0; c.patch.ops[0].lvl = 1; c.patch.matrix[0][0] = 8; }},
      {"NS SHORT", -29.9, true, [](StationCore &c) {
         for (auto &o : c.patch.ops) { o.on = 0; o.lvl = 0; }
         c.patch.noise = StationCore::Noise{1, 1, 0.35, 0, 1, 0, StationCore::Env{1, 120, 1, 80, 0, 0}};
       }},
      {"PLS raw, pw 0.1", -1.9, true, [](StationCore &c) { c.patch.ops[0].wave = 3; c.patch.ops[0].lvl = 1; c.patch.ops[0].pure = 0; c.patch.ops[0].pw = 0.1; }},
    };
    bool onOk = true, ctrlOk = true;
    for (const auto &cf : cfgs)
    {
      double db[2];
      for (int b = 0; b < 2; b++)
      {
        StationCore c = blank();
        c.dcBlock = (b == 0);
        for (int i = 0; i < 3; i++) c.patch.ops[i].on = (i == 0);
        cf.mut(c);
        c.noteOn(60, noteFreq(60));
        const std::vector<double> y = pull(c, 48000 * 2);
        // The SECOND half only. The blocker's own step response has tau =
        // 1/(2pi*5) = 32 ms, so a mean taken across note-on measures the
        // transient rather than the rejection -- the PLS row reads -37 dB over
        // the whole window and far lower once settled. The bypass leg uses the
        // identical window, so the S17 comparison stays like-for-like.
        const std::vector<double> tail(y.begin() + (long)(y.size() / 2), y.end());
        db[b] = 20 * std::log10(std::max(std::fabs(mean(tail)), 1e-300)) -
                20 * std::log10(std::max(peak(tail), 1e-300));
      }
      std::printf("   %-20s  blocker ON %7.1f dB     BYPASS %7.1f dB  (%s %.1f)\n",
                  cf.name, db[0], db[1], cf.pinned ? "S17 pin" : "control, gate <=", cf.pin);
      onOk = onOk && db[0] <= -60;
      ctrlOk = ctrlOk && (cf.pinned ? std::fabs(db[1] - cf.pin) <= 3.0 : db[1] <= cf.pin);
    }
    row(onOk, "DC blocker: every configuration <= -60 dB below peak once settled", nullptr, 0);
    row(ctrlOk, "control: bypass reproduces labharness S17's pinned DC (+-3 dB), controls read the floor", nullptr, 0);
  }

  std::printf("-- the five deliberate divergences (SPEC-STATION §11) --\n");

  // 3.11 §11.2 RATIO continuous. The lab's 0.5-stepped values are exact points
  // of this law (parity above proves them); what is new is that BETWEEN them the
  // frequency is monotonic and correct. The control is a repeated ratio, which
  // must read the SAME frequency — a monotonicity probe that cannot detect
  // equality would call any noisy estimator monotonic.
  {
    auto fOf = [](double ratio) {
      StationCore c = blank();
      for (int i = 0; i < 3; i++) { c.patch.ops[i].on = (i == 0); c.patch.ops[i].lvl = (i == 0) ? 1 : 0; }
      c.patch.ops[0].coarse = ratio;
      c.noteOn(48, noteFreq(48));
      return zcFreq(pull(c, 48000), 48000);
    };
    bool mono = true;
    double worstErr = 0, prev = 0;
    for (int k = 0; k <= 60; k++)
    {
      const double ratio = 0.25 + k * (16.0 - 0.25) / 60.0;
      const double got = fOf(ratio), want = noteFreq(48) * ratio;
      worstErr = std::max(worstErr, std::fabs(got - want) / want * 100);
      if (got <= prev) mono = false;
      prev = got;
    }
    const double a = fOf(3.25), b = fOf(3.25);
    row(mono && worstErr < 0.05 && a == b,
        "RATIO is continuous: 61 points over 0.25..16 monotonic and exact",
        "worst frequency error %.4f %%", worstErr);
  }

  // 3.12 §11.4 release-fade stealing. The lab hard-shifts at 8 voices (a measured
  // 68 %-of-peak step). The 17th note here must steal without a step above the
  // signal's own slope. CONTROL: a 5th note with only 4 held steals nothing and
  // must read the natural floor, so the probe is not simply reporting "small".
  {
    auto stealStep = [](int held, int extra) {
      StationCore c = blank();
      for (int i = 0; i < 3; i++) { c.patch.ops[i].on = (i == 0); c.patch.ops[i].lvl = (i == 0) ? 1 : 0; }
      for (int i = 0; i < held; i++) c.noteOn(60 + i, noteFreq(60 + i));
      const std::vector<double> a = pull(c, 24000);
      const double before = a.back(), nat = std::fabs(a[a.size() - 1] - a[a.size() - 2]);
      c.noteOn(100 + extra, noteFreq(100 + extra));
      // The FIRST sample after the event, exactly as labharness S19 measures it.
      // A max over a window instead would also catch the new note's own attack,
      // which is signal, not a click -- and the control proved that: it read
      // 13.3x on a case where nothing is stolen at all.
      const std::vector<double> b = pull(c, 8);
      return std::tuple<double, double, double>{std::fabs(b[0] - before), nat, peak(a)};
    };
    const auto s16 = stealStep(16, 0);
    const auto s4 = stealStep(4, 1);
    std::printf("   17th note on 16 held: worst step %.4e vs natural %.4e (%.1fx), peak %.4e\n",
                std::get<0>(s16), std::get<1>(s16), std::get<0>(s16) / std::get<1>(s16), std::get<2>(s16));
    std::printf("   control  5th note on 4 held (no steal): %.4e vs natural %.4e (%.1fx)\n",
                std::get<0>(s4), std::get<1>(s4), std::get<0>(s4) / std::get<1>(s4));
    row(std::get<0>(s16) <= 2 * std::get<1>(s16) && std::get<0>(s4) <= 2 * std::get<1>(s4),
        "voice steal is click-free: no step above 2x the signal's own slope",
        "steal is %.1f %% of peak (lab: 68 %%)", std::get<0>(s16) / std::get<2>(s16) * 100);
  }

  // 3.13 §11.3 FREE / RING / STEPPED. Each must be bit-INERT when off (or the
  // default patch is not the default patch) and measurably different when on.
  // The "on" half is a must-not-read-zero control: a feature wired to nothing
  // would pass the inertness half perfectly.
  {
    auto renderWith = [](void (*mut)(StationCore &)) {
      StationCore c = blank();
      for (int i = 0; i < 3; i++) { c.patch.ops[i].on = 1; c.patch.ops[i].lvl = (i == 0) ? 0.85 : 0; }
      c.patch.ops[1].coarse = 2;
      c.patch.ops[2].coarse = 14;
      c.patch.matrix[1][0] = 2.6;
      c.patch.matrix[2][0] = 1.1;
      c.patch.ops[0].env = StationCore::Env{3, 420, 0.55, 260, 0, 0};
      if (mut) mut(c);
      c.noteOn(60, noteFreq(60));
      return pull(c, 24000);
    };
    const std::vector<double> base = renderWith(nullptr);
    auto diff = [&](const std::vector<double> &x) {
      double d = 0;
      for (size_t i = 0; i < base.size(); i++) d = std::max(d, std::fabs(base[i] - x[i]));
      return d;
    };
    // FREE has to run a few thousand samples before it differs from RETRIG at
    // phase 0, so the probe renders first and THEN takes a second note.
    StationCore cf = blank();
    for (int i = 0; i < 3; i++) { cf.patch.ops[i].on = (i == 0); cf.patch.ops[i].lvl = (i == 0) ? 1 : 0; }
    cf.patch.ops[0].retrig = 0;
    cf.noteOn(60, noteFreq(60));
    pull(cf, 7777);
    cf.noteOn(64, noteFreq(64));
    const double freeStart = cf.voiceAt(1).ph[0];

    const double dRingOff = diff(renderWith([](StationCore &c) { c.patch.ops[1].ring = StationCore::kRingOff; }));
    const double dStepOff = diff(renderWith([](StationCore &c) { c.patch.ops[0].env.step = 0; }));
    const double dRingOn = diff(renderWith([](StationCore &c) { c.patch.ops[1].lvl = 0.5; c.patch.ops[1].ring = StationCore::kRingOp1; }));
    const double dStepOn = diff(renderWith([](StationCore &c) { c.patch.ops[0].env.step = 4; }));
    const double dPhase = diff(renderWith([](StationCore &c) { c.patch.ops[0].phase = 0.25; }));
    std::printf("   OFF (must be 0): ring %.1e  stepped %.1e      ON (must not be 0): ring %.3e  stepped %.3e  phase-offset %.3e\n",
                dRingOff, dStepOff, dRingOn, dStepOn, dPhase);
    std::printf("   FREE: after 7777 samples a new voice starts at phase %.6f, not at 0 (RETRIG would be 0)\n", freeStart);
    row(dRingOff == 0 && dStepOff == 0 && dRingOn > 1e-3 && dStepOn > 1e-3 && dPhase > 1e-3 && freeStart > 1e-3,
        "FREE / RING / STEPPED: bit-inert off, measurably different on", nullptr, 0);
  }

  // 3.14 §11.1 DRW pure branch band-limiting. The audit's -43.7 dB at MIDI 96 is
  // an FFT number and there is no FFT harness on the C++ side, so this row reads
  // the SAME quantity one bin at a time with a Goertzel: the table's harmonics
  // that fall above Nyquist and fold back are alias bins by construction, and a
  // band-limited branch must not have them. CONTROL: the raw ZOH branch, through
  // the identical detector, must read them loudly -- a detector that finds no
  // aliasing in a 32-step zero-order hold is broken, not reassuring.
  {
    const int midi = 96;
    const double sr = 48000, f = noteFreq(midi);
    auto renderDrw = [&](double pure) {
      StationCore c = blank(sr);
      c.dcBlock = false;
      for (int i = 0; i < 3; i++) { c.patch.ops[i].on = (i == 0); c.patch.ops[i].lvl = (i == 0) ? 1 : 0; }
      c.patch.ops[0].wave = StationCore::kDrw;
      c.patch.ops[0].pure = pure;
      c.patch.seed = 1024;
      c.loadFactoryTable(StationCore::kRnd);
      c.noteOn(midi, noteFreq(midi));
      return pull(c, 1 << 16);
    };
    const std::vector<double> bl = renderDrw(1), zoh = renderDrw(0);
    auto aliasFloorDb = [&](const std::vector<double> &x) {
      const double fund = goertzel(x, sr, f);
      double worst = 0;
      for (int n = 2; n < 40; n++)
      {
        double fa = n * f;
        while (fa > sr * 0.5) fa = std::fabs(sr - fa);                   // fold into [0, Nyquist]
        if (std::fabs(fa - std::round(fa / f) * f) < f * 0.05) continue;  // a true harmonic, not an alias
        worst = std::max(worst, goertzel(x, sr, fa));
      }
      return 20 * std::log10(std::max(worst, 1e-300) / std::max(fund, 1e-300));
    };
    const double dbBl = aliasFloorDb(bl), dbZoh = aliasFloorDb(zoh);
    std::printf("   MIDI 96 DRW worst alias bin, dB below the fundamental:  band-limited %.1f   raw ZOH %.1f   (lab lerp: -43.7)\n",
                dbBl, dbZoh);
    row(dbBl < dbZoh - 12 && dbBl < -43.7 && rms(bl) > 1e-3,
        "DRW pure is band-limited (mipmap) and beats the lab's -43.7 dB lerp; control: raw ZOH aliases",
        "band-limited alias floor %.1f dB", dbBl);
  }

  // 3.15 §4's 5 ms cell smoothing. The lab has NO smoothing anywhere (audit S6
  // measured a matrix write at 13.9x the signal's own slope), so this is
  // build-side work with nothing to match. CONTROL: op LVL, which SPEC §4 does
  // NOT ask to be smoothed, must still step hard through the same probe — the
  // audit found it needs smoothing too (S6), and a detector that read both as
  // smooth would be measuring nothing.
  {
    auto stepOf = [](bool matrixWrite) {
      StationCore c = blank();
      for (int i = 0; i < 3; i++) { c.patch.ops[i].on = 1; c.patch.ops[i].lvl = (i == 0) ? 0.9 : 0; }
      c.patch.ops[1].coarse = 2;
      c.patch.matrix[1][0] = 2.6;
      c.noteOn(60, noteFreq(60));
      const std::vector<double> a = pull(c, 24000);
      const double before = a.back(), nat = std::fabs(a[a.size() - 1] - a[a.size() - 2]);
      if (matrixWrite) c.patch.matrix[1][0] = 8.0;
      else c.patch.ops[0].lvl = 0.1;
      const std::vector<double> b = pull(c, 8);
      return std::pair<double, double>{std::fabs(b[0] - before), nat};
    };
    const auto sm = stepOf(true), lv = stepOf(false);
    std::printf("   matrix cell 2.6 -> 8.0 (smoothed): step %.4e vs natural %.4e (%.2fx; lab reads 13.9x)\n",
                sm.first, sm.second, sm.first / sm.second);
    std::printf("   control  op LVL 0.9 -> 0.1 (NOT smoothed, audit S6 says it should be): %.4e (%.2fx the natural slope)\n",
                lv.first, lv.first / lv.second);
    row(sm.first <= 2 * sm.second && lv.first > 2 * lv.second,
        "matrix cells smooth over 5 ms (§4); control: an unsmoothed write still steps",
        "cell step %.2fx the natural slope", sm.first / sm.second);
  }

  // ==================================================================== 4. CPU
  // REPORTED, NEVER GATED — the absolute number is machine-dependent. The audit
  // (§3.4) measured the Node lab at 18.71 % of realtime for 16 voices on the max
  // patch and states the falsifiable form: SPEC §12's <= ~2 % is met iff this
  // core is >= 9.4x the Node core.
  {
    const int secs = 5, N = 48000 * secs;
    double best = 1e30;
    for (int t = 0; t < 3; t++)
    {
      StationCore c = maxPatch();
      for (int i = 0; i < StationCore::kPoly; i++) c.noteOn(48 + i, noteFreq(48 + i));
      std::vector<float> L(256), R(256);
      const auto t0 = std::chrono::steady_clock::now();
      for (int off = 0; off < N; off += 256) c.render(L.data(), R.data(), std::min(256, N - off));
      const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      best = std::min(best, el);
    }
    const double pct = best / secs * 100;
    std::printf("-- CPU (REPORT, not gated) --\n");
    std::printf("   16 voices, max patch, 48 kHz, 5 s, min of 3: %.3f s  =  %.2f %% of one core\n", best, pct);
    std::printf("   SPEC §12 budget <= ~2 %%; audit §3.4 Node lab reads 18.71 %% (needs >= 9.4x) -> this build is %.1fx the lab\n",
                18.71 / std::max(pct, 1e-9));
  }

  std::printf("station_check: %s (%d failure%s; worst parity rms %.3e)\n",
              g_failures ? "RED" : "GREEN", g_failures, g_failures == 1 ? "" : "s", worst);
  return g_failures ? 1 : 0;
}
