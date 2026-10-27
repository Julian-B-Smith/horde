// harness.cpp — the hosted-module admission test's rows (ROADMAP B561, ADR-295), written once
// against admission::Module (module.h) and run against each module's adapter.
//
// The specification is docs/proposals/rack-slot-contract.md §2.2, read with its §12 amendments.
// This file runs the MODULE-ONLY rows: A1-A5, A6 tests 1-3 and 7 (docs/proposals/module-io-gain.md),
// A8-A12, A15, A17-A20, and A21 for a module that declares morphPosition. The slot rows (A6 tests
// 4-6, A7, A13, A14, A16) need horde 2's shell (B398) and are not here. CPU is never a gate here
// (ADR-187 §8; RS-8.3): the one timing this file prints (--timing) is reported, not judged.
//
// OUTPUT, one line each, parsed by tools/admission_check.py:
//   FAIL <row> <tag> :: <detail>    a sub-check that failed. <tag> is stable across runs (no
//                                   measured numbers in it), so the driver can tell a plant's new
//                                   failure from one the clean module already had.
//   NOTE <row> :: <text>            a reported reading, never judged
//   DETAIL <row> :: <text>          a per-case reading, for the report file only
//   ROW <row> <PASS|FAIL|NOTRUN|NA> :: <summary>
//
// HOW EACH RULE IS JUDGED, AND WHY EXACT. Every gated comparison here is exact (bit-identical
// renders, exact zero, integer sample counts, set membership) or structural (a ramp within one of
// its own steps of the ideal line), with no free threshold: the contract states none, and a
// threshold invented here would be a tolerance nobody ruled (docs/armor/tolerances.json). Where
// the contract asks for a reading it does not judge (ADR-218's click readings), it is printed.
//
//   admission_harness --module scape --tree DIR [--rows A1,A2,...] [--plant ID] [--no-readings]
//                     [--manifest-json] [--timing] [--rtsan control|session]
//                     [--list-plants]
#include <algorithm>
#include <array>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "module.h"
#include "plants.h"

namespace admission {
ModuleInfo scapeModule(const std::string& tree);
}

#ifdef ADMISSION_RTSAN
// Exported by the RealtimeSanitizer runtime (not in its installed interface header): the pair
// [[clang::nonblocking]] functions are instrumented with. Everything between them is judged.
extern "C" {
void __rtsan_realtime_enter(void);
void __rtsan_realtime_exit(void);
}
#endif

// ---- allocation counter (row A19: "refused before allocation") ---------------------------------
static long g_allocs = 0;
static bool g_armed = false;
void* operator new(std::size_t n) {
  if (g_armed) ++g_allocs;
  if (void* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace admission;

constexpr double kPi = 3.14159265358979323846;

// mulberry32 (SPEC §5.7: the only RNG stream in this project), mapped to [0, 1).
struct Rng {
  std::uint32_t s;
  double next() {
    std::uint32_t t = s += 0x6D2B79F5u;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
  }
};

struct Buf {
  std::vector<Sample> L, R;
  explicit Buf(std::size_t n = 0) : L(n, 0), R(n, 0) {}
  std::size_t size() const { return L.size(); }
};

std::size_t secs(double s, double sr) { return static_cast<std::size_t>(std::lround(s * sr)); }
std::string num(double v) { char b[48]; std::snprintf(b, sizeof b, "%.6g", v); return b; }
std::string rate(double sr) { return num(sr / 1000) + "k"; }

Buf noise(std::size_t n, std::uint32_t seed, double amp = 0.5) {
  Buf b(n);
  Rng r{seed};
  for (std::size_t i = 0; i < n; ++i) { b.L[i] = amp * (2 * r.next() - 1); b.R[i] = amp * (2 * r.next() - 1); }
  return b;
}
Buf sines(std::size_t n, double sr, double fL, double fR, double amp) {
  Buf b(n);
  for (std::size_t i = 0; i < n; ++i) {
    b.L[i] = amp * std::sin(2 * kPi * fL * static_cast<double>(i) / sr);
    b.R[i] = amp * std::sin(2 * kPi * fR * static_cast<double>(i) / sr);
  }
  return b;
}
// ADR-218's chord probe: sines at 55, 110, 130.81 and 164.81 Hz, equal amplitude, the sum
// peaking at -6 dBFS (scaled to its own measured peak over the render), mono.
Buf chordProbe(std::size_t n, double sr) {
  Buf b(n);
  const double f[4] = {55, 110, 130.81, 164.81};
  double peak = 0;
  for (std::size_t i = 0; i < n; ++i) {
    double s = 0;
    for (double x : f) s += std::sin(2 * kPi * x * static_cast<double>(i) / sr);
    b.L[i] = s;
    peak = std::max(peak, std::fabs(s));
  }
  const double g = std::pow(10, -6.0 / 20) / peak;
  for (std::size_t i = 0; i < n; ++i) b.R[i] = b.L[i] = b.L[i] * g;
  return b;
}
Buf constant(std::size_t n, double v) {
  Buf b(n);
  std::fill(b.L.begin(), b.L.end(), v);
  std::fill(b.R.begin(), b.R.end(), v);
  return b;
}

// Render in[from, to) into out[from, to) with block sizes taken cyclically from `splits`.
void run(Module& m, const Buf& in, Buf& out, std::size_t from, std::size_t to, const std::vector<std::uint32_t>& splits) {
  std::size_t i = from, k = 0;
  while (i < to) {
    const std::uint32_t n = static_cast<std::uint32_t>(std::min<std::size_t>(splits[k++ % splits.size()], to - i));
    m.process(&in.L[i], &in.R[i], &out.L[i], &out.R[i], n);
    i += n;
  }
}
Buf render(Module& m, const Buf& in, std::uint32_t block) {
  Buf out(in.size());
  run(m, in, out, 0, in.size(), {block});
  return out;
}

// Bit-identical: compares the bits (so -0 and +0 differ, and a NaN equals only itself).
bool same(const Buf& a, const Buf& b, std::size_t* first = nullptr) {
  if (a.size() != b.size()) { if (first) *first = 0; return false; }
  for (std::size_t i = 0; i < a.size(); ++i)
    if (std::memcmp(&a.L[i], &b.L[i], sizeof(Sample)) != 0 || std::memcmp(&a.R[i], &b.R[i], sizeof(Sample)) != 0) {
      if (first) *first = i;
      return false;
    }
  return true;
}

void applyTo(Module& m, const Settings& s) {
  for (const auto& [a, v] : s) m.setParam(m.resolve(a), v);
}
Settings targets(const Module& m) {
  Settings t;
  for (const auto& s : m.manifest()) t.emplace_back(s.address, m.getParam(m.resolve(s.address)));
  return t;
}
Settings join(Settings a, const Settings& b) { a.insert(a.end(), b.begin(), b.end()); return a; }
// Every flip-class key at its other end (for Scape: the Vintage engine).
Settings flipped(const Module& m) {
  Settings s;
  for (const auto& p : m.manifest())
    if (p.cls == Cls::Flip) s.emplace_back(p.address, m.getParam(m.resolve(p.address)) == p.min ? p.max : p.min);
  return s;
}
// A move of every key from where it is now: continuous keys by a tenth of their span (wrapping
// back from the top), stepped keys to their other end. Applied mid-stream it puts a ramp, a
// crossfade or a switch in flight on every key at once.
Settings moveAll(const Module& m) {
  Settings s;
  for (const auto& p : m.manifest()) {
    const double v = m.getParam(m.resolve(p.address)), span = p.max - p.min;
    if (p.steps > 0) s.emplace_back(p.address, v == p.min ? p.max : p.min);
    else s.emplace_back(p.address, v + span / 10 <= p.max ? v + span / 10 : v - span / 10);
  }
  return s;
}

struct Ctx {
  std::function<std::unique_ptr<Module>()> make;
  std::vector<std::pair<std::string, std::string>> presets;
  bool readings = true;
};
std::unique_ptr<Module> fresh(const Ctx& c, const Settings& s, double sr, std::uint32_t mb) {
  auto m = c.make();
  applyTo(*m, s);   // set before prepare: what a slot restoring a session does
  m->prepare(sr, mb);
  return m;
}
Settings presetSettings(const Ctx& c, const std::string& text) {
  auto m = c.make();
  Patch p;
  if (!m->parsePatch(text, p, nullptr)) return {};
  return p.values;
}
// The cases most rows run: the module's defaults, its stress probe, and the stress probe with
// every flip key at its other end.
std::vector<std::pair<std::string, Settings>> baseCases(const Ctx& c) {
  auto m = c.make();
  const Settings st = m->stressProbe();
  auto f = c.make();
  applyTo(*f, st);
  return {{"default", {}}, {"stress", st}, {"stress+flipped", join(st, flipped(*f))}};
}

struct Out {
  std::string row;
  std::vector<std::pair<std::string, std::string>> fails;
  std::vector<std::string> notes, details;
  std::string status, summary;   // status "" = PASS or FAIL from `fails`
  void fail(const std::string& tag, const std::string& d) { fails.emplace_back(tag, d); }
  void note(const std::string& t) { notes.push_back(t); }
  void print() const {
    for (const auto& [t, d] : fails) std::printf("FAIL %s %s :: %s\n", row.c_str(), t.c_str(), d.c_str());
    for (const auto& n : notes) std::printf("NOTE %s :: %s\n", row.c_str(), n.c_str());
    for (const auto& n : details) std::printf("DETAIL %s :: %s\n", row.c_str(), n.c_str());
    const std::string st = !status.empty() ? status : fails.empty() ? "PASS" : "FAIL";
    std::printf("ROW %s %s :: %s\n", row.c_str(), st.c_str(), summary.c_str());
    std::fflush(stdout);
  }
};

const char* violationName(foundations::OperatorProperties::Violation v) {
  using V = foundations::OperatorProperties::Violation;
  switch (v) {
    case V::kNone: return "none";
    case V::kLatencyUndeclared: return "latency undeclared";
    case V::kTailUndeclared: return "tail undeclared";
    case V::kMinBlockUndeclared: return "min_block undeclared";
    case V::kZeroMinBlock: return "min_block 0";
    case V::kResidualNotFinite: return "residual not finite";
    case V::kResidualOutOfRange: return "residual out of [0, 0.5]";
    case V::kResidualWithoutSign: return "residual without sign";
    case V::kSignWithoutResidual: return "sign without residual";
    case V::kTieRoundedDown: return "tie rounded down";
  }
  return "?";
}
const char* clsName(Cls c) { return c == Cls::Morph ? "morph" : c == Cls::Flip ? "flip" : c == Cls::Device ? "device" : "none"; }
const char* switchName(Switch s) {
  switch (s) {
    case Switch::Undeclared: return "undeclared";
    case Switch::EqualGainCrossfade: return "equal-gain crossfade";
    case Switch::Gate: return "gate";
    case Switch::SeededJump: return "seeded jump";
    case Switch::Silent: return "silent";
    case Switch::Dip: return "dip (duck / swap / un-duck inside the module)";
    case Switch::Jump: return "bare jump";
  }
  return "?";
}

// ================================================================================================
// A1 Declarations
Out rowA1(const Ctx& c) {
  Out o{"A1"};
  for (double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
    auto m = c.make();
    if (!m->prepare(sr, 4096)) { o.fail("prepare@" + rate(sr), "prepare() refused"); continue; }
    const auto p = m->properties();
    const auto chk = p.validate();
    if (!chk) o.fail("validate@" + rate(sr), std::string("FOUNDATIONS validate(): ") + violationName(chk.violation));
    else if (sr == 48000)
      o.note("OperatorProperties at 48k: latency " + std::to_string(p.latency->samples) + ", tail " +
             (p.tail->samples == foundations::kInfiniteTail ? std::string("infinite") : std::to_string(p.tail->samples)) +
             (p.tail->may_change ? " (may_change)" : "") + ", min_block " + std::to_string(*p.min_block) +
             (p.load ? ", LoadDecl " + std::to_string(p.load->transition_samples) : ", no LoadDecl"));
  }
  auto m = c.make();
  m->prepare(48000, 4096);
  const auto spec = m->manifest();
  if (spec.empty()) o.fail("manifest", "no parameters declared");
  std::set<std::string> addrs;
  std::set<int> handles;
  std::map<Cls, int> byCls;
  for (const auto& s : spec) {
    ++byCls[s.cls];
    if (s.cls == Cls::None) o.fail("class[" + s.address + "]", "no class (morph / flip / Device)");
    if (!s.rateDeclared) o.fail("rate[" + s.address + "]", "no rate class");
    if (!s.unitDeclared) o.fail("unit[" + s.address + "]", "no unit");
    if (!(s.min < s.max)) o.fail("range[" + s.address + "]", "range [" + num(s.min) + ", " + num(s.max) + "] is empty");
    if (s.def < s.min || s.def > s.max) o.fail("default[" + s.address + "]", "default " + num(s.def) + " outside its range");
    if (!addrs.insert(s.address).second) o.fail("duplicate[" + s.address + "]", "address declared twice");
    const int h = m->resolve(s.address);
    if (h < 0) o.fail("resolve[" + s.address + "]", "the declared address does not resolve");
    else if (!handles.insert(h).second) o.fail("handle[" + s.address + "]", "two addresses resolve to one handle");
  }
  if (m->resolve("no.such.key") >= 0) o.fail("resolve-unknown", "an undeclared address resolved");
  const auto d = m->declared();
  o.note(std::to_string(spec.size()) + " parameters: " + std::to_string(byCls[Cls::Morph]) + " morph, " + std::to_string(byCls[Cls::Flip]) +
         " flip, " + std::to_string(byCls[Cls::Device]) + " Device; changes_image " + (d.changesImage ? "true" : "false") +
         ", changes_level " + (d.changesLevel ? "true" : "false") + "; morphPosition " + (d.morphPosition ? "declared" : "not declared"));
  o.summary = "validate() at 44.1/48/96/192k; " + std::to_string(spec.size()) + " parameters, every one classed, ranged and resolvable";
  return o;
}

// ================================================================================================
// A2 Reset identity: a reset() mid-move must equal a freshly prepared instance with the same
// parameters, bit for bit. The move puts a ramp, a crossfade or a switch in flight on every key,
// so a smoother the reset leaves unsnapped is in flight when it happens.
Out rowA2(const Ctx& c) {
  Out o{"A2"};
  int n = 0;
  for (const auto& [name, s] : baseCases(c)) {
    for (double sr : {48000.0, 96000.0}) {
      auto a = fresh(c, s, sr, 512);
      render(*a, noise(secs(0.5, sr), 11), 256);
      applyTo(*a, moveAll(*a));
      render(*a, noise(secs(0.007, sr), 12), 64);
      a->reset();
      auto b = fresh(c, targets(*a), sr, 512);
      const Buf in = noise(secs(2, sr), 13);
      const Buf ya = render(*a, in, 256), yb = render(*b, in, 256);
      std::size_t first = 0;
      if (!same(ya, yb, &first)) o.fail("reset[" + name + "@" + rate(sr) + "]", "after reset() mid-move, differs from a fresh instance from sample " + std::to_string(first));
      ++n;
    }
  }
  o.summary = std::to_string(n) + " cases (default, stress, stress+flipped at 48k and 96k), 2 s of seeded noise after a reset mid-move";
  return o;
}

// ================================================================================================
// A3 Block independence: the same render split at n = 1, 7, 64, 256, 4096 and three seeded
// random split sequences, with every key moving in flight across the split points, must equal
// the render made in one call per segment.
Out rowA3(const Ctx& c) {
  Out o{"A3"};
  const double sr = 48000;
  const std::size_t n1 = 4096, n2 = 16384;
  int runs = 0;
  for (const auto& [name, s] : baseCases(c)) {
    if (name == "stress") continue;
    auto probe = fresh(c, s, sr, n2);
    const auto mb = probe->properties().min_block.value_or(1);
    const Settings moves = moveAll(*probe);
    Buf in(n1 + n2);
    const Buf a = noise(n1, 21), b = noise(n2, 22);
    std::copy(a.L.begin(), a.L.end(), in.L.begin()); std::copy(a.R.begin(), a.R.end(), in.R.begin());
    std::copy(b.L.begin(), b.L.end(), in.L.begin() + n1); std::copy(b.R.begin(), b.R.end(), in.R.begin() + n1);
    auto go = [&](const std::vector<std::uint32_t>& sp) {
      auto m = fresh(c, s, sr, n2);
      Buf out(n1 + n2);
      run(*m, in, out, 0, n1, sp);
      applyTo(*m, moves);
      run(*m, in, out, n1, n1 + n2, sp);
      return out;
    };
    const Buf ref = go({static_cast<std::uint32_t>(n2)});
    std::vector<std::pair<std::string, std::vector<std::uint32_t>>> splits;
    for (std::uint32_t k : {1u, 7u, 64u, 256u, 4096u})
      if (k >= mb) splits.push_back({"n=" + std::to_string(k), {k}});
    for (std::uint32_t seed : {1u, 2u, 3u}) {
      Rng r{seed};
      std::vector<std::uint32_t> v;
      for (int i = 0; i < 4096; ++i) v.push_back(std::max<std::uint32_t>(mb, 1 + static_cast<std::uint32_t>(r.next() * 1024)));
      splits.push_back({"random" + std::to_string(seed), v});
    }
    for (const auto& [sn, sp] : splits) {
      std::size_t first = 0;
      if (!same(go(sp), ref, &first)) o.fail("split[" + name + "," + sn + "]", "differs from the one-call render from sample " + std::to_string(first));
      ++runs;
    }
  }
  o.summary = std::to_string(runs) + " split renders (default and stress+flipped at 48k), every key moving across the splits";
  return o;
}

// ================================================================================================
// A4 Latency truth, read on the module's dry setting where the output is the input delayed by the
// latency: the impulse's peak and the swept sine's cross-correlation peak must sit at the declared
// samples (or one off, on the side the declared residual's sign names). On every patch, no output
// may come before the declared latency.
Out rowA4(const Ctx& c) {
  Out o{"A4"};
  auto probe = c.make();
  const Settings dry = probe->dryProbe();
  if (dry.empty()) { o.status = "NOTRUN"; o.summary = "the adapter names no dry setting to read a delay on"; return o; }
  auto allowed = [](const foundations::LatencyDecl& l, long lag) {
    const long L = static_cast<long>(l.samples);
    if (lag == L) return true;
    if (l.residual > 0 && l.sign == foundations::ResidualSign::kLate && lag == L + 1) return true;
    if (l.residual > 0 && l.sign == foundations::ResidualSign::kEarly && lag == L - 1) return true;
    return false;
  };
  const std::size_t k0 = 256;
  auto firstNonzero = [](const Buf& y) {
    for (std::size_t i = 0; i < y.size(); ++i)
      if (y.L[i] != 0 || y.R[i] != 0) return static_cast<long>(i);
    return -1L;
  };
  for (double sr : {48000.0, 96000.0}) {
    auto m = fresh(c, dry, sr, 4096);
    const auto p = m->properties();
    if (!p.latency) { o.fail("declared@" + rate(sr), "latency undeclared"); continue; }
    Buf imp(8192);
    imp.L[k0] = imp.R[k0] = 1;
    const Buf h = render(*m, imp, 512);
    std::size_t peak = 0;
    for (std::size_t i = 0; i < h.size(); ++i)
      if (std::fabs(h.L[i]) > std::fabs(h.L[peak])) peak = i;
    const long lagI = static_cast<long>(peak) - static_cast<long>(k0);
    if (!allowed(*p.latency, lagI)) o.fail("impulse@" + rate(sr), "impulse peak at " + std::to_string(lagI) + " samples, declared " + std::to_string(p.latency->samples));
    // swept sine, 20 Hz to 0.45 x the rate, logarithmic, over 0.5 s
    auto m2 = fresh(c, dry, sr, 4096);
    const std::size_t n = secs(0.5, sr);
    Buf x(n);
    const double f0 = 20, f1 = 0.45 * sr, T = static_cast<double>(n) / sr, kk = std::log(f1 / f0);
    for (std::size_t i = 0; i < n; ++i) {
      const double t = static_cast<double>(i) / sr;
      x.L[i] = x.R[i] = 0.5 * std::sin(2 * kPi * f0 * T / kk * (std::exp(t / T * kk) - 1));
    }
    const Buf y = render(*m2, x, 512);
    long best = 0;
    double bestV = -1;
    for (long lag = -64; lag <= 64; ++lag) {
      double acc = 0;
      for (std::size_t i = 64; i + 64 < n; ++i) {
        const std::size_t j = static_cast<std::size_t>(static_cast<long>(i) + lag);
        acc += x.L[i] * y.L[j] + x.R[i] * y.R[j];
      }
      if (acc > bestV) { bestV = acc; best = lag; }
    }
    if (!allowed(*p.latency, best)) o.fail("sweep@" + rate(sr), "swept-sine cross-correlation peaks at " + std::to_string(best) + " samples, declared " + std::to_string(p.latency->samples));
  }
  // causality on every patch: nothing before the declared latency (less one sample if the
  // residual is declared early)
  std::vector<std::pair<std::string, Settings>> patches = {{"default", {}}};
  for (const auto& [f, t] : c.presets) patches.emplace_back(f, presetSettings(c, t));
  for (const auto& [name, s] : patches) {
    auto m = fresh(c, s, 48000, 4096);
    const auto p = m->properties();
    if (!p.latency) continue;
    const long early = p.latency->residual > 0 && p.latency->sign == foundations::ResidualSign::kEarly ? 1 : 0;
    Buf imp(4096);
    imp.L[k0] = imp.R[k0] = 1;
    const long f = firstNonzero(render(*m, imp, 512));
    if (f >= 0 && f - static_cast<long>(k0) < static_cast<long>(p.latency->samples) - early)
      o.fail("early[" + name + "]", "output " + std::to_string(static_cast<long>(p.latency->samples) - (f - static_cast<long>(k0))) + " samples before the declared latency");
  }
  o.summary = "impulse and swept sine on the dry setting at 48k and 96k; causality on the defaults and " + std::to_string(c.presets.size()) + " factory presets";
  return o;
}

// ================================================================================================
// A5 Silence and tails. Silence in from the start: exact-zero out. After 1 s of input, the output
// reaches exact zero within the declared tail (read when the input stops: may_change) and stays
// there for the rest of the window. A denormal residue never reaches exact zero.
Out rowA5(const Ctx& c) {
  Out o{"A5"};
  std::vector<std::pair<std::string, Settings>> cases = baseCases(c);
  for (const auto& [f, t] : c.presets) cases.emplace_back(f, presetSettings(c, t));
  for (const auto& [name, s] : cases) {
    auto m = fresh(c, s, 48000, 512);
    const Buf y = render(*m, Buf(secs(1, 48000)), 512);
    for (std::size_t i = 0; i < y.size(); ++i)
      if (y.L[i] != 0 || y.R[i] != 0) { o.fail("silence[" + name + "]", "silence in, non-zero out at sample " + std::to_string(i)); break; }
  }
  // the preset with the shortest declared tail, for the high rates
  std::string shortest;
  Settings shortS;
  std::uint32_t shortT = foundations::kInfiniteTail;
  for (const auto& [f, t] : c.presets) {
    auto m = fresh(c, presetSettings(c, t), 48000, 512);
    const auto p = m->properties();
    if (p.tail && p.tail->samples < shortT) { shortT = p.tail->samples; shortest = f; shortS = presetSettings(c, t); }
  }
  struct T { std::string name; Settings s; double sr; double window; };   // window 0: declared tail + 1 s
  const auto bc = baseCases(c);
  std::vector<T> tails = {{"default", {}, 44100, 60}, {"default", {}, 48000, 60}, {"stress+flipped", bc[2].second, 48000, 60}};
  if (!shortest.empty()) { tails.push_back({shortest, shortS, 96000, 0}); tails.push_back({shortest, shortS, 192000, 0}); }
  int judged = 0;
  for (const auto& t : tails) {
    auto m = fresh(c, t.s, t.sr, 512);
    render(*m, noise(secs(1, t.sr), 31), 512);
    const auto p = m->properties();
    const std::string tag = t.name + "@" + rate(t.sr);
    if (!p.tail) { o.fail("tail-declared[" + tag + "]", "tail undeclared"); continue; }
    if (p.tail->samples == foundations::kInfiniteTail) { o.note(tag + ": tail declared infinite; not judged"); continue; }
    const std::size_t window = t.window > 0 ? secs(t.window, t.sr) : p.tail->samples + secs(1, t.sr);
    if (p.tail->samples > window) { o.note(tag + ": declared tail " + num(p.tail->samples / t.sr) + " s exceeds the " + num(window / t.sr) + " s window; not judged"); continue; }
    const Buf z(window);
    Buf y(window);
    run(*m, z, y, 0, window, {512});
    long last = -1;
    for (std::size_t i = 0; i < window; ++i)
      if (y.L[i] != 0 || y.R[i] != 0) last = static_cast<long>(i);
    ++judged;
    if (last >= static_cast<long>(p.tail->samples))
      o.fail("tail[" + tag + "]", "non-zero output " + num((last + 1) / t.sr) + " s after the input stopped; declared tail " + num(p.tail->samples / t.sr) + " s");
    else
      o.details.push_back(tag + ": exact zero after " + num((last + 1) / t.sr) + " s, " + num(100.0 * (last + 1) / p.tail->samples) + " % of the declared " + num(p.tail->samples / t.sr) + " s");
  }
  o.summary = "silence from the start on " + std::to_string(cases.size()) + " patches at 48k; " + std::to_string(judged) + " tails judged (44.1/48k in a 60 s window; " +
              (shortest.empty() ? std::string("no preset") : shortest) + " at 96/192k over its declared tail + 1 s)";
  return o;
}

// ================================================================================================
// A6 I/O gain, tests 1-3 and 7 of docs/proposals/module-io-gain.md, read on the dry setting with a
// constant input of 0.25: the output is then exactly 0.25 x the applied gain (a power of two
// scales exactly), so the gain is read sample for sample with no estimate.
Out rowA6(const Ctx& c) {
  Out o{"A6"};
  auto probe = c.make();
  const Settings dry = probe->dryProbe();
  const std::string keys[2] = {"io.inGain", "io.outGain"};
  bool both = true;
  for (const auto& k : keys) {
    bool found = false;
    for (const auto& s : probe->manifest()) {
      if (s.address != k) continue;
      found = true;
      if (s.unit != "dB" || s.min != -24 || s.max != 24 || s.def != 0 || s.cls != Cls::Device)
        o.fail("law[" + k + "]", "declared " + s.unit + " [" + num(s.min) + ", " + num(s.max) + "] default " + num(s.def) + " " + clsName(s.cls) + "; the standard is dB [-24, 24] default 0, Device");
    }
    both = both && found;
  }
  if (!both) { o.status = "NA"; o.summary = "the module declares no I/O gain pair: the slot provides it (test 6, a slot row)"; return o; }
  if (dry.empty()) { o.status = "NOTRUN"; o.summary = "the adapter names no dry setting"; return o; }
  const double sr0 = 48000;
  // ---- test 1: 0 dB is exactly unity
  {
    auto m = fresh(c, join(dry, {{keys[0], 0}, {keys[1], 0}}), sr0, 512);
    const Buf in = noise(secs(0.5, sr0), 41);
    std::size_t first = 0;
    if (!same(render(*m, in, 512), in, &first)) o.fail("t1.dry", "at 0 dB on the dry setting the output is not the input, from sample " + std::to_string(first));
    auto x = fresh(c, {}, sr0, 512), y = fresh(c, {{keys[0], 0}, {keys[1], 0}}, sr0, 512);
    applyTo(*y, {{keys[0], 0}, {keys[1], 0}});
    if (!same(render(*x, in, 512), render(*y, in, 512), &first)) o.fail("t1.explicit", "0 dB set explicitly differs from the gains never touched, from sample " + std::to_string(first));
  }
  // ---- test 2: a step 0 -> +6 dB is a linear amplitude ramp over 20 ms +- one sample
  for (double sr : {44100.0, 48000.0, 96000.0}) {
    for (const auto& k : keys) {
      auto m = fresh(c, join(dry, {{keys[0], 0}, {keys[1], 0}}), sr, 512);
      const std::size_t pre = secs(0.1, sr), n = pre + secs(0.1, sr);
      const Buf in = constant(n, 0.25);
      Buf y(n);
      run(*m, in, y, 0, pre, {64});
      m->setParam(m->resolve(k), 6);
      run(*m, in, y, pre, n, {64});
      std::vector<double> g(n);
      for (std::size_t i = 0; i < n; ++i) g[i] = y.L[i] * 4;
      const double g0 = g[pre - 1], g1 = g[n - 1];
      const std::string tag = k + "@" + rate(sr);
      if (g0 != 1 || g1 == g0) { o.fail("t2.levels[" + tag + "]", "before the step " + num(g0) + ", after " + num(g1) + " (must be 1, then the +6 dB gain)"); continue; }
      std::size_t s = n - 1;
      while (s > pre && g[s - 1] == g1) --s;
      const long N = static_cast<long>(s - pre) + 1, ideal = std::lround(0.02 * sr);
      if (N < ideal - 1 || N > ideal + 1) o.fail("t2.length[" + tag + "]", "the ramp takes " + std::to_string(N) + " samples; 20 ms is " + std::to_string(ideal));
      const double step = std::fabs(g1 - g0) / static_cast<double>(N);
      long off = -1, back = -1;
      for (long j = 0; j + 1 < N; ++j) {
        const double ideal_g = g0 + (g1 - g0) * static_cast<double>(j + 1) / static_cast<double>(N);
        if (off < 0 && std::fabs(g[pre + static_cast<std::size_t>(j)] - ideal_g) > step) off = j;
        if (back < 0 && j > 0 && (g[pre + static_cast<std::size_t>(j)] - g[pre + static_cast<std::size_t>(j) - 1]) * (g1 - g0) < 0) back = j;
      }
      if (off >= 0) o.fail("t2.linear[" + tag + "]", "ramp sample " + std::to_string(off) + " is more than one step off the straight line");
      if (back >= 0) o.fail("t2.monotone[" + tag + "]", "the ramp reverses at sample " + std::to_string(back));
      if (sr == sr0) o.details.push_back("t2 " + tag + ": " + std::to_string(N) + " samples to +6 dB (20 ms = " + std::to_string(ideal) + ")");
    }
  }
  // ---- test 3: after prepare at a saved +6 dB, the first sample is already at +6 dB; and after
  // a state load the same. The reference is the same gain reached by a ramp and settled.
  for (const auto& k : keys) {
    auto ref = fresh(c, join(dry, {{keys[0], 0}, {keys[1], 0}}), sr0, 512);
    render(*ref, constant(64, 0.25), 64);
    ref->setParam(ref->resolve(k), 6);
    const Buf yr = render(*ref, constant(secs(0.1, sr0), 0.25), 64);
    const double settled = yr.L.back() * 4;
    auto m = fresh(c, join(dry, {{keys[0], 0}, {keys[1], 0}, {k, 6}}), sr0, 512);
    const double first = render(*m, constant(1, 0.25), 1).L[0] * 4;
    if (first != settled) o.fail("t3.prepare[" + k + "]", "first sample after prepare at +6 dB is at gain " + num(first) + ", settled +6 dB is " + num(settled));
    std::vector<char> st(64 * 1024);
    const std::size_t len = ref->saveState(st.data(), st.size());
    auto l = fresh(c, join(dry, {{keys[0], 0}, {keys[1], 0}}), sr0, 512);
    render(*l, constant(256, 0.25), 64);
    if (!len || !l->stageState(std::string_view(st.data(), len)) || !l->adoptStaged()) { o.fail("t3.load[" + k + "]", "the state did not load"); continue; }
    const double afterLoad = render(*l, constant(1, 0.25), 1).L[0] * 4;
    if (afterLoad != settled) o.fail("t3.load[" + k + "]", "first sample after a state load at +6 dB is at gain " + num(afterLoad) + ", settled +6 dB is " + num(settled));
  }
  // ---- test 7: both clip latches set on an over and hold until cleared
  {
    auto m = fresh(c, join(dry, {{keys[0], 0}, {keys[1], 0}}), sr0, 512);
    render(*m, constant(secs(0.1, sr0), 0.5), 256);
    Meters mt = m->meters();
    if (mt.inClip || mt.outClip) o.fail("t7.false-latch", "a latch set with no over");
    Buf over = constant(256, 0.5);
    over.L[10] = over.R[11] = 2.0;
    render(*m, over, 256);
    render(*m, constant(secs(1, sr0), 0.5), 256);
    mt = m->meters();
    if (!mt.inClip || !mt.outClip) o.fail("t7.hold", std::string("1 s after an over: input latch ") + (mt.inClip ? "held" : "CLEARED") + ", output latch " + (mt.outClip ? "held" : "CLEARED"));
    m->clearMeters();
    mt = m->meters();
    if (mt.inClip || mt.outClip) o.fail("t7.clear", "clearMeters() left a latch set");
    m->setParam(m->resolve(keys[1]), 12);   // 0.5 x +12 dB > 1 on the output only
    render(*m, constant(secs(0.2, sr0), 0.5), 256);
    mt = m->meters();
    if (mt.inClip || !mt.outClip) o.fail("t7.independent", std::string("an output-only over: input latch ") + (mt.inClip ? "set" : "clear") + ", output latch " + (mt.outClip ? "set" : "clear"));
  }
  o.summary = "test 1 (0 dB exact, dry and explicit), test 2 (20 ms linear ramp at 44.1/48/96k, both trims), test 3 (opens at the saved gain, prepare and load), test 7 (latches hold until cleared)";
  return o;
}

// ================================================================================================
// A8 Stereo image: L 220 Hz, R 330 Hz. |L - R| must not collapse unless changes_image is
// declared. "Collapse" is judged exactly (the output's L - R is zero throughout); the contract
// gives no partial-collapse figure, so the side level is reported, not judged.
Out rowA8(const Ctx& c) {
  Out o{"A8"};
  const double sr = 48000;
  const std::size_t n = secs(2, sr), from = secs(1, sr);
  const Buf in = sines(n, sr, 220, 330, 0.5);
  auto sideRms = [&](const Buf& b) {
    double s = 0;
    for (std::size_t i = from; i < n; ++i) s += (b.L[i] - b.R[i]) * (b.L[i] - b.R[i]);
    return std::sqrt(s / static_cast<double>(n - from));
  };
  const double sIn = sideRms(in);
  auto m0 = c.make();
  const bool declared = m0->declared().changesImage;
  std::vector<std::pair<std::string, Settings>> patches = {{"default", {}}};
  for (const auto& [f, t] : c.presets) patches.emplace_back(f, presetSettings(c, t));
  double worst = INFINITY;   // the lowest side level seen, dB; -inf is an exact collapse
  std::string worstName;
  for (const auto& [name, s] : patches) {
    auto m = fresh(c, s, sr, 512);
    const double sOut = sideRms(render(*m, in, 512));
    if (!declared && sOut == 0) o.fail("collapse[" + name + "]", "L - R is exactly zero at the output and changes_image is not declared");
    const double db = sOut > 0 ? 20 * std::log10(sOut / sIn) : -INFINITY;
    o.details.push_back(name + ": side " + (std::isfinite(db) ? num(db) + " dB" : std::string("-inf dB")) + " re the input's");
    if (db < worst) { worst = db; worstName = name; }
  }
  o.note(std::string("changes_image ") + (declared ? "declared: the rule is met by declaration" : "NOT declared") + "; lowest side level " +
         (std::isfinite(worst) ? num(worst) + " dB" : std::string("-inf dB")) + " re the input's (" + worstName + ")");
  o.summary = "L 220 Hz / R 330 Hz on the defaults and " + std::to_string(c.presets.size()) + " presets";
  return o;
}

// ================================================================================================
// A9 Non-finite: NaN, +Inf and -Inf at the input, each in its own block, and one NaN planted in
// the module's state. The output stays finite; each episode is counted and latches; a cleared
// latch clears; the module recovers and keeps sounding.
Out rowA9(const Ctx& c) {
  Out o{"A9"};
  const double sr = 48000;
  auto finite = [](const Buf& b) {
    for (std::size_t i = 0; i < b.size(); ++i)
      if (!std::isfinite(b.L[i]) || !std::isfinite(b.R[i])) return false;
    return true;
  };
  const auto bc = baseCases(c);
  bool stateRan = false;
  for (const auto& [name, s] : {bc[0], bc[2]}) {
    auto m = fresh(c, s, sr, 512);
    render(*m, noise(secs(0.2, sr), 51), 256);
    struct E { const char* what; double vL, vR; };
    const E eps[3] = {{"NaN-L", std::nan(""), 0.1}, {"+Inf-R", 0.1, INFINITY}, {"-Inf-both", -INFINITY, -INFINITY}};
    bool allFinite = true;
    for (const auto& e : eps) {
      m->clearNonFiniteLatch();
      const auto c0 = m->nonFiniteCount();
      Buf blk = noise(256, 52);
      blk.L[17] = e.vL;
      blk.R[17] = e.vR;
      allFinite = finite(render(*m, blk, 256)) && allFinite;
      allFinite = finite(render(*m, noise(secs(0.1, sr), 53), 256)) && allFinite;
      const std::string tag = std::string(e.what) + "," + name;
      if (m->nonFiniteCount() <= c0) o.fail("count[" + tag + "]", "a non-finite input was not counted");
      if (!m->nonFiniteLatched()) o.fail("latch[" + tag + "]", "a non-finite input did not latch");
      m->clearNonFiniteLatch();
      if (m->nonFiniteLatched()) o.fail("clear[" + tag + "]", "the latch did not clear");
    }
    if (!allFinite) o.fail("finite-input[" + name + "]", "non-finite output after a non-finite input");
    // the state plant
    m->clearNonFiniteLatch();
    const auto c0 = m->nonFiniteCount();
    if (!m->plantNonFiniteState()) { o.note(name + ": the adapter has no seam to plant a non-finite value in state; that half NOT RUN"); continue; }
    stateRan = true;
    bool fin = true;
    std::size_t ran = 0;
    const Buf z(64);
    while (m->nonFiniteCount() == c0 && ran < secs(2, sr)) { fin = finite(render(*m, z, 64)) && fin; ran += 64; }
    fin = finite(render(*m, Buf(secs(0.2, sr)), 256)) && fin;
    if (m->nonFiniteCount() == c0) o.fail("state-count[" + name + "]", "a NaN in state was never counted (2 s)");
    if (!m->nonFiniteLatched()) o.fail("state-latch[" + name + "]", "a NaN in state did not latch");
    const Buf after = render(*m, noise(secs(0.5, sr), 54), 256);
    fin = finite(after) && fin;
    if (!fin) o.fail("state-finite[" + name + "]", "non-finite output after a NaN in state");
    bool sounds = false;
    for (std::size_t i = 0; i < after.size() && !sounds; ++i) sounds = after.L[i] != 0;
    if (!sounds) o.fail("state-recover[" + name + "]", "silent after the guard fired");
    o.details.push_back(name + ": state NaN counted after " + std::to_string(ran) + " samples; total count " + std::to_string(m->nonFiniteCount()));
  }
  o.summary = std::string("3 input episodes on the defaults and stress+flipped; state plant ") + (stateRan ? "run" : "NOT RUN");
  return o;
}

// ================================================================================================
// The B265 click metric, ported from docs/design/fx-chain-morph-lab-r3.html (clickOf). Reported
// only: ADR-218 suspended its 3 dB ceiling as a pass/fail rule.
double clickDb(const std::vector<Sample>& y, double sr, std::size_t t0) {
  const std::size_t W = std::max<std::size_t>(4, secs(0.002, sr)), n = y.size() / W;
  std::vector<double> e(n), r(n);
  double gmax = 0;
  for (std::size_t k = 0; k < n; ++k) {
    double s = 0;
    for (std::size_t i = k * W; i < (k + 1) * W; ++i) {
      if (i < 2) continue;
      const double d = y[i] - 2 * y[i - 1] + y[i - 2];
      s += d * d;
    }
    e[k] = std::sqrt(s / static_cast<double>(W));
    gmax = std::max(gmax, e[k]);
  }
  const double tiny = std::max(1e-12, 1e-6 * gmax);
  std::vector<double> buf;
  for (std::size_t k = 0; k < n; ++k) {
    buf.clear();
    for (std::size_t j = k >= 16 ? k - 16 : 0; j <= std::min(n - 1, k + 16); ++j)
      if (j != k) buf.push_back(e[j]);
    std::sort(buf.begin(), buf.end());
    const double med = buf.empty() ? e[k] : buf[buf.size() >> 1];
    r[k] = 20 * std::log10(std::max(tiny, e[k]) / std::max(tiny, med));
  }
  const std::size_t k0 = t0 / W, ks = secs(0.25, sr) / W, kspan = secs(1.0, sr) / W;
  double pre = 0, post = 0, peak = -1e300;
  for (std::size_t k = k0 >= ks ? k0 - ks : 0; k < k0; ++k) pre = std::max(pre, r[k]);
  for (std::size_t k = k0 + kspan; k < std::min(n, k0 + kspan + ks); ++k) post = std::max(post, r[k]);
  for (std::size_t k = k0; k < std::min(n, k0 + kspan); ++k) peak = std::max(peak, r[k]);
  return std::max(0.0, peak - std::max(pre, post));
}

// ================================================================================================
// A11 Parameter classes, as amended by ADR-218 and ADR-234 (§12.1, §12.2).
//  flip-class, structural: each flip key declares an admitted switch method; a stepped key is
//    never declared morph (the slot would blend it); a value between two steps renders as the
//    nearest step; a key declared silent is proven silent (a flip renders bit-identical to none).
//  morph-class, "no step": a move by setParam must not render bit-identically to the same move
//    landed at once through the load entry (a jump). A move the landed render cannot hear at the
//    probe is reported as silent. The step's SIZE is not judged: no ratified figure exists.
//  Device keys: never in a written patch, never moved by a patch load.
//  REPORTED (never judged): the B265 click reading of every non-silent flip at 48k on the two
//    pinned probes, at the defaults and each factory preset, both directions, beside an ideal
//    offline equal-gain crossfade of the two steady renders over the declared length.
Out rowA11(const Ctx& c) {
  Out o{"A11"};
  const double sr = 48000;
  auto m0 = c.make();
  const auto spec = m0->manifest();
  const Settings stress = m0->stressProbe();
  int flips = 0, morphs = 0, silentMoves = 0;
  for (const auto& s : spec) {
    if (s.steps > 0 && s.cls == Cls::Morph) o.fail("blended[" + s.address + "]", "a stepped key (" + std::to_string(s.steps) + " values) declared morph-class: the slot would blend it");
    if (s.cls != Cls::Flip) continue;
    ++flips;
    if (s.steps == 0) o.fail("flip-continuous[" + s.address + "]", "a flip-class key with a continuous range");
    const bool admitted = (s.sw == Switch::EqualGainCrossfade && s.transitionS > 0) || s.sw == Switch::Gate || s.sw == Switch::SeededJump || s.sw == Switch::Silent;
    if (!admitted)
      o.fail("method[" + s.address + "]", std::string("switch method ") + switchName(s.sw) + (s.switchSource.empty() ? "" : " (" + s.switchSource + ")") +
                                              ", length " + num(s.transitionS * 1000) + " ms: not an admitted method (equal-gain crossfade, gate, declared seeded jump)");
    if (s.sw == Switch::SeededJump) o.note(s.address + ": a declared seeded jump; its switch-table reading and its unseeded-jump control are the module's own (ADR-234), not re-run here");
  }
  // stepped keys snap: a quarter of the way between the first two steps renders as the first
  const Buf probe = sines(secs(0.5, sr), sr, 1000, 1500, std::pow(10, -6.0 / 20));
  for (const auto& s : spec) {
    if (s.steps < 2 || s.cls == Cls::Device) continue;
    const double s0 = s.min, s1 = s.min + (s.max - s.min) / (s.steps - 1);
    auto a = fresh(c, join(stress, {{s.address, s0}}), sr, 512), b = fresh(c, join(stress, {{s.address, s0 + (s1 - s0) / 4}}), sr, 512);
    if (!same(render(*a, probe, 512), render(*b, probe, 512))) o.fail("snap[" + s.address + "]", "a value between two steps does not render as the nearest step");
  }
  // silent flips are proven silent
  for (const auto& s : spec) {
    if (s.cls != Cls::Flip || s.sw != Switch::Silent) continue;
    const Buf in = sines(secs(1, sr), sr, 1000, 1500, std::pow(10, -6.0 / 20));
    auto a = fresh(c, stress, sr, 512), b = fresh(c, stress, sr, 512);
    Buf ya(in.size()), yb(in.size());
    const std::size_t half = in.size() / 2;
    run(*a, in, ya, 0, half, {512});
    run(*b, in, yb, 0, half, {512});
    const double v = b->getParam(b->resolve(s.address));
    b->setParam(b->resolve(s.address), v == s.min ? s.max : s.min);
    run(*a, in, ya, half, in.size(), {512});
    run(*b, in, yb, half, in.size(), {512});
    std::size_t first = 0;
    if (!same(ya, yb, &first)) o.fail("silent[" + s.address + "]", "declared silent, but a flip changes the output from sample " + std::to_string(first - half) + " after it");
  }
  // morph-class: ramped, not landed
  for (const auto& s : spec) {
    if (s.cls != Cls::Morph || s.steps > 0) continue;
    ++morphs;
    const Buf in = sines(secs(0.45, sr), sr, 1000, 1500, std::pow(10, -6.0 / 20));
    const std::size_t at = secs(0.3, sr);
    auto st = fresh(c, stress, sr, 512), ra = fresh(c, stress, sr, 512), la = fresh(c, stress, sr, 512);
    Buf ys(in.size()), yr(in.size()), yl(in.size());
    run(*st, in, ys, 0, at, {256});
    run(*ra, in, yr, 0, at, {256});
    run(*la, in, yl, 0, at, {256});
    const double v = ra->getParam(ra->resolve(s.address));
    const double to = (s.max - v) > (v - s.min) ? s.max : s.min;
    ra->setParam(ra->resolve(s.address), to);
    Patch p;
    p.name = "landed";
    for (const auto& q : spec)
      if (q.cls != Cls::Device) p.values.emplace_back(q.address, q.address == s.address ? to : la->getParam(la->resolve(q.address)));
    if (!la->stagePatch(p) || !la->adoptStaged()) { o.fail("land[" + s.address + "]", "the load entry refused a one-key move"); continue; }
    run(*st, in, ys, at, in.size(), {256});
    run(*ra, in, yr, at, in.size(), {256});
    run(*la, in, yl, at, in.size(), {256});
    if (same(yl, ys)) { ++silentMoves; o.details.push_back("morph " + s.address + ": the move is silent at the probe (a landed move equals no move)"); continue; }
    if (same(yr, yl)) o.fail("step[" + s.address + "]", "a setParam move renders bit-identically to the same move landed at once: no smoothing");
  }
  // Device keys: never in a patch, never moved by a load
  {
    auto m = fresh(c, {}, sr, 512);
    Settings dev;
    for (const auto& s : spec)
      if (s.cls == Cls::Device) dev.emplace_back(s.address, s.def + (s.max - s.def) / 4);
    applyTo(*m, dev);
    render(*m, Buf(512), 512);
    std::vector<std::pair<std::string, std::string>> texts = c.presets;
    if (!c.presets.empty()) {
      std::string crafted = c.presets.front().second;
      for (const auto& [a, v] : dev) crafted += a + " " + num(v + 1) + "\n";
      texts.emplace_back("crafted (a patch text carrying the Device keys)", crafted);
    }
    for (const auto& [name, text] : texts) {
      Patch p;
      if (!m->parsePatch(text, p, nullptr)) continue;   // a refused patch moves nothing
      const std::string w = m->writePatch(p);
      for (const auto& [a, v] : dev) {
        bool inValues = false;
        for (const auto& pv : p.values) inValues = inValues || pv.first == a;
        if (w.find(a + " ") != std::string::npos || inValues) o.fail("device-in-preset[" + a + "]", "Device key " + a + " is stored in a patch (" + name + ")");
      }
      if (!m->stagePatch(p) || !m->adoptStaged()) continue;
      for (const auto& [a, v] : dev)
        if (m->getParam(m->resolve(a)) != v) o.fail("device-moved[" + a + "]", "loading a patch moved Device key " + a + " (" + name + ")");
    }
  }
  // the readings
  int readings = 0;
  if (c.readings) {
    std::vector<std::pair<std::string, Settings>> patches = {{"default", {}}};
    for (const auto& [f, t] : c.presets) patches.emplace_back(f, presetSettings(c, t));
    const std::size_t t0 = secs(1.0, sr), n = t0 + secs(1.25, sr);
    const std::pair<std::string, Buf> probes[2] = {{"chord", chordProbe(n, sr)}, {"1 kHz", sines(n, sr, 1000, 1000, std::pow(10, -6.0 / 20))}};
    for (const auto& s : spec) {
      if (s.cls != Cls::Flip || s.sw == Switch::Silent) continue;
      const std::size_t xf = std::max<std::size_t>(1, secs(s.transitionS, sr));
      double worstFlip = 0, worstIdeal = 0, worstOver = -1e300;
      std::string worstAt;
      for (const auto& [pname, ps] : patches) {
        for (const auto& [prname, pr] : probes) {
          Buf steady[2];
          for (int e = 0; e < 2; ++e) { auto m = fresh(c, join(ps, {{s.address, e ? s.max : s.min}}), sr, 512); steady[e] = render(*m, pr, 512); }
          for (int dir = 0; dir < 2; ++dir) {
            auto m = fresh(c, join(ps, {{s.address, dir ? s.max : s.min}}), sr, 512);
            Buf y(n);
            run(*m, pr, y, 0, t0, {512});
            m->setParam(m->resolve(s.address), dir ? s.min : s.max);
            run(*m, pr, y, t0, n, {512});
            const Buf& A = steady[dir];
            const Buf& B = steady[1 - dir];
            Buf ideal(n);
            for (std::size_t i = 0; i < n; ++i) {
              const double w = i < t0 ? 0 : std::min(1.0, static_cast<double>(i - t0 + 1) / static_cast<double>(xf));
              ideal.L[i] = A.L[i] * (1 - w) + B.L[i] * w;
              ideal.R[i] = A.R[i] * (1 - w) + B.R[i] * w;
            }
            const double f = std::max(clickDb(y.L, sr, t0), clickDb(y.R, sr, t0));
            const double id = std::max(clickDb(ideal.L, sr, t0), clickDb(ideal.R, sr, t0));
            const std::string where = pname + ", " + prname + ", " + (dir ? "max->min" : "min->max");
            o.details.push_back("flip " + s.address + " " + where + ": " + num(f) + " dB (ideal equal-gain " + num(s.transitionS * 1000) + " ms: " + num(id) + " dB)");
            worstFlip = std::max(worstFlip, f);
            worstIdeal = std::max(worstIdeal, id);
            if (f - id > worstOver) { worstOver = f - id; worstAt = where; }
            ++readings;
          }
        }
      }
      o.note("click reading (REPORTED, ADR-218), " + s.address + " over " + std::to_string(patches.size()) + " patches x 2 probes x 2 directions: worst " + num(worstFlip) +
             " dB; ideal equal-gain crossfade worst " + num(worstIdeal) + " dB; largest excess over its ideal " + num(worstOver) + " dB (" + worstAt + ")");
    }
  }
  o.summary = std::to_string(flips) + " flip keys (method, snap, silence), " + std::to_string(morphs) + " morph keys ramped-not-landed (" + std::to_string(silentMoves) +
              " silent at the probe), Device keys over " + std::to_string(c.presets.size()) + " presets; " + std::to_string(readings) + " click readings reported";
  return o;
}

// ================================================================================================
// A12 Macros: every factory preset, and a synthetic three-slot patch (the factory set may carry no
// macros, and a round trip of nothing proves nothing), parse -> write -> parse -> write: the
// structure (order, labels, homes, bindings, curves) and the text both round-trip exactly; every
// curve returns lo at 0 and hi at 1 exactly; a `log` curve across zero fails validation.
Out rowA12(const Ctx& c) {
  Out o{"A12"};
  auto m = c.make();
  int slots = 0;
  auto roundTrip = [&](const std::string& name, const std::string& text) {
    Patch p, p1;
    std::string err;
    if (!m->parsePatch(text, p, &err)) { o.fail("parse[" + name + "]", "refused: " + err); return; }
    const std::string t1 = m->writePatch(p);
    if (!m->parsePatch(t1, p1, &err)) { o.fail("reparse[" + name + "]", "its own written text is refused: " + err); return; }
    if (!(p1 == p)) o.fail("roundtrip[" + name + "]", "parse(write(p)) differs from p (order, labels, homes, bindings, curves or values)");
    if (m->writePatch(p1) != t1) o.fail("text[" + name + "]", "write(parse(write(p))) differs from write(p)");
    for (const auto& mc : p.macros) {
      ++slots;
      for (const auto& b : mc.bind)
        if (m->macroValue(b, 0) != b.lo || m->macroValue(b, 1) != b.hi) o.fail("endpoints[" + name + "]", "binding on " + b.address + " does not return lo and hi exactly at 0 and 1");
    }
  };
  for (const auto& [f, t] : c.presets) roundTrip(f, t);
  // the synthetic patch: three slots on the first three continuous morph keys whose range holds 0,
  // and that are not in Hz (a Hz key's contribution may be in octaves)
  std::vector<ParamSpec> keys;
  for (const auto& s : m->manifest())
    if (s.cls == Cls::Morph && s.steps == 0 && s.unit != "Hz" && s.min <= 0 && 0 <= s.max && keys.size() < 3) keys.push_back(s);
  if (keys.size() == 3 && !c.presets.empty()) {
    Patch base;
    m->parsePatch(c.presets.front().second, base, nullptr);
    base.name = "Admission macro probe";
    const char* labels[3] = {"Alpha", "Beta", "Gamma"};
    const char* curves[3] = {"lin", "exp", "lin"};
    for (int i = 0; i < 3; ++i) base.macros.push_back({labels[i], 0.25 * (i + 1), {{keys[static_cast<std::size_t>(i)].address, 0, (keys[static_cast<std::size_t>(i)].max - keys[static_cast<std::size_t>(i)].min) / 4, curves[i]}}});
    const std::string text = m->writePatch(base);
    Patch back;
    if (!m->parsePatch(text, back, nullptr)) o.fail("synthetic.parse", "the synthetic macro patch is refused");
    else {
      if (!(back.macros == base.macros)) o.fail("synthetic.roundtrip", "the synthetic patch's macros do not round-trip (order, labels, homes, bindings or curves)");
      roundTrip("synthetic", text);
    }
    Patch bad = base;
    bad.macros.resize(1);
    bad.macros[0].bind[0].lo = -bad.macros[0].bind[0].hi;
    bad.macros[0].bind[0].curve = "log";
    Patch out;
    if (m->parsePatch(m->writePatch(bad), out, nullptr)) o.fail("log-accepted", "a log curve across zero passed validation");
  } else {
    o.note("no three continuous non-Hz morph keys spanning 0: the synthetic macro patch was not built");
  }
  o.summary = std::to_string(c.presets.size()) + " factory presets and a synthetic 3-slot patch; " + std::to_string(slots) + " macro slots round-tripped";
  return o;
}

// ================================================================================================
// A15 Determinism and isolation (B230): two instances, same input, rendered side by side block by
// block, are bit-identical to each other and to one rendered alone; an instance beside one fed a
// different input is bit-identical to itself alone.
Out rowA15(const Ctx& c) {
  Out o{"A15"};
  const auto bc = baseCases(c);
  int n = 0;
  for (const auto& [name, s] : {bc[0], bc[2]}) {
    for (double sr : {48000.0, 96000.0}) {
      const std::size_t len = secs(1, sr);
      const Buf x = noise(len, 61), y = noise(len, 62);
      std::vector<std::uint32_t> sp;
      Rng r{7};
      for (int i = 0; i < 4096; ++i) sp.push_back(1 + static_cast<std::uint32_t>(r.next() * 512));
      const std::size_t moveAt = len / 3;
      auto solo = [&](const Buf& in) {
        auto m = fresh(c, s, sr, 512);
        Buf out(len);
        run(*m, in, out, 0, moveAt, sp);
        applyTo(*m, moveAll(*m));
        run(*m, in, out, moveAt, len, sp);
        return out;
      };
      const Buf alone = solo(x);
      auto pair = [&](const Buf& inB, Buf& outA, Buf& outB) {
        auto a = fresh(c, s, sr, 512), b = fresh(c, s, sr, 512);
        std::size_t i = 0, k = 0;
        bool moved = false;
        while (i < len) {
          if (!moved && i >= moveAt) { applyTo(*a, moveAll(*a)); applyTo(*b, moveAll(*b)); moved = true; }
          std::uint32_t blk = static_cast<std::uint32_t>(std::min<std::size_t>(sp[k++ % sp.size()], len - i));
          if (!moved && i + blk > moveAt) blk = static_cast<std::uint32_t>(moveAt - i);
          a->process(&x.L[i], &x.R[i], &outA.L[i], &outA.R[i], blk);
          b->process(&inB.L[i], &inB.R[i], &outB.L[i], &outB.R[i], blk);
          i += blk;
        }
      };
      const std::string tag = name + "@" + rate(sr);
      Buf a1(len), b1(len), a2(len), b2(len);
      pair(x, a1, b1);
      if (!same(a1, b1)) o.fail("twin[" + tag + "]", "two instances with the same input differ when rendered side by side");
      if (!same(a1, alone)) o.fail("side-by-side[" + tag + "]", "an instance rendered beside a twin differs from it rendered alone");
      pair(y, a2, b2);
      if (!same(a2, alone)) o.fail("isolation[" + tag + "]", "an instance beside one fed a different input differs from it alone");
      n += 3;
    }
  }
  o.summary = std::to_string(n) + " comparisons (defaults and stress+flipped at 48k and 96k, random blocks, every key moving mid-render)";
  return o;
}

// ================================================================================================
// State helpers for A17-A20.
std::string save(const Module& m) {
  std::vector<char> b(64 * 1024);
  const std::size_t n = m.saveState(b.data(), b.size());
  return std::string(b.data(), n);
}
bool load(Module& m, const std::string& t) { return m.stageState(t) && m.adoptStaged(); }
// Every key at a seeded value inside its range (a stepped key at a seeded step).
Settings randomSettings(const Module& m, std::uint32_t seed) {
  Settings s;
  Rng r{seed};
  for (const auto& p : m.manifest()) {
    const double u = 0.2 + 0.6 * r.next();
    if (p.steps > 1) s.emplace_back(p.address, p.min + (p.max - p.min) * static_cast<double>(static_cast<int>(r.next() * p.steps)) / (p.steps - 1));
    else s.emplace_back(p.address, p.min + (p.max - p.min) * u);
  }
  return s;
}
// A session with history: every key at a seeded value, input, then a move left in flight.
std::unique_ptr<Module> session(const Ctx& c, double sr) {
  auto probe = c.make();
  auto m = fresh(c, randomSettings(*probe, 71), sr, 512);
  render(*m, noise(secs(0.5, sr), 72), 256);
  applyTo(*m, moveAll(*m));
  render(*m, noise(secs(0.005, sr), 73), 64);
  return m;
}

// A17 State completeness: state saved mid-session, loaded into a fresh instance; the original
// reset(); the two render bit-identically from there on.
Out rowA17(const Ctx& c) {
  Out o{"A17"};
  const double sr = 48000;
  auto a = session(c, sr);
  const std::string st = save(*a);
  auto b = fresh(c, {}, sr, 512);
  if (st.empty() || !load(*b, st)) { o.fail("load", "the saved state did not load"); return o; }
  a->reset();
  const Buf in = noise(secs(1, sr), 74);
  std::size_t first = 0;
  if (!same(render(*a, in, 256), render(*b, in, 256), &first)) o.fail("complete", "a fresh instance loaded from the saved state differs from the reset original from sample " + std::to_string(first));
  o.summary = "every key at a seeded value, a move in flight; state " + std::to_string(st.size()) + " bytes";
  return o;
}

// A18 Restore exactness: save -> load -> save is byte-identical; every value restores exactly;
// the renders after each load are bit-identical (and equal the reset original). Then the same
// under a comma-decimal host locale (the human's 2026-10-27 scope for Scape's Q-027: every locale).
Out rowA18(const Ctx& c) {
  Out o{"A18"};
  const double sr = 48000;
  auto a = session(c, sr);
  const std::string s0 = save(*a);
  auto b = fresh(c, {}, sr, 512);
  if (!load(*b, s0)) { o.fail("load", "the saved state did not load"); return o; }
  const std::string s1 = save(*b);
  if (s1 != s0) o.fail("text", "save -> load -> save is not byte-identical");
  for (const auto& p : a->manifest())
    if (a->getParam(a->resolve(p.address)) != b->getParam(b->resolve(p.address)))
      o.fail("values[" + p.address + "]", "restored " + num(b->getParam(b->resolve(p.address))) + ", saved " + num(a->getParam(a->resolve(p.address))));
  auto c2 = fresh(c, {}, sr, 512);
  load(*c2, s1);
  a->reset();
  const Buf in = noise(secs(0.5, sr), 75);
  const Buf ya = render(*a, in, 256), yb = render(*b, in, 256), yc = render(*c2, in, 256);
  if (!same(yb, yc)) o.fail("render", "the renders after the two loads differ");
  if (!same(ya, yb)) o.fail("render-original", "the render after a load differs from the reset original");
  // the locale half
  const char* de[] = {"de_DE.UTF-8", "de_DE", "de_DE.ISO8859-1"};
  const char* got = nullptr;
  for (const char* l : de)
    if (std::setlocale(LC_ALL, l)) { got = l; break; }
  if (!got) o.note("no comma-decimal locale on this machine: the locale half NOT RUN");
  else {
    const std::string sd = save(*b);
    auto d = fresh(c, {}, sr, 512);
    const bool ok = load(*d, s0);
    std::setlocale(LC_ALL, "C");
    if (sd != s0) o.fail("locale.text", std::string("under ") + got + " the saved state differs from the C locale's");
    if (!ok) o.fail("locale.load", std::string("under ") + got + " a C-locale state is refused");
    else
      for (const auto& p : a->manifest())
        if (d->getParam(d->resolve(p.address)) != b->getParam(b->resolve(p.address))) { o.fail("locale.values", std::string("under ") + got + " a C-locale state restores " + p.address + " inexactly"); break; }
  }
  o.summary = std::string("seeded session, every key; locale half ") + (got ? std::string("under ") + got : std::string("NOT RUN"));
  return o;
}

// A19 State size: every factory preset's state, and a worst-case state, fit the 2 KiB budget
// (RS-6.12) and the module's own; an oversized state is refused with no allocation.
Out rowA19(const Ctx& c) {
  Out o{"A19"};
  const double sr = 48000;
  const std::size_t kBudget = 2048;   // RS-6.12, provisional (Q9)
  auto m0 = c.make();
  const std::size_t own = m0->stateBudget();
  if (own > kBudget) o.fail("budget", "the module's own budget " + std::to_string(own) + " exceeds 2048 bytes");
  const std::size_t cap = std::min(own, kBudget);
  std::size_t biggest = 0;
  for (const auto& [f, t] : c.presets) {
    auto m = fresh(c, {}, sr, 512);
    Patch p;
    if (!m->parsePatch(t, p, nullptr) || !m->stagePatch(p) || !m->adoptStaged()) { o.fail("preset[" + f + "]", "the preset did not load"); continue; }
    const std::size_t n = save(*m).size();
    biggest = std::max(biggest, n);
    if (n > cap) o.fail("size[" + f + "]", std::to_string(n) + " bytes over the " + std::to_string(cap) + "-byte budget");
  }
  // worst case: every value with seventeen significant digits, negative where the range allows
  Settings w;
  for (const auto& p : m0->manifest()) w.emplace_back(p.address, p.steps > 0 ? p.max : p.min + (p.max - p.min) * 0.123456789012345678);
  auto m = fresh(c, w, sr, 512);
  const std::string ws = save(*m);
  if (ws.size() > cap) o.fail("worst", "a worst-case state is " + std::to_string(ws.size()) + " bytes, over " + std::to_string(cap));
  // oversized: refused, and nothing allocated while refusing
  std::string big = ws;
  big.append(own + 1 - std::min(own, big.size()), '\n');
  g_allocs = 0;
  g_armed = true;
  const bool took = m->stageState(big);
  g_armed = false;
  const long allocs = g_allocs;
  if (took) o.fail("oversize-accepted", "a " + std::to_string(big.size()) + "-byte state was accepted");
  if (allocs != 0) o.fail("oversize-allocated", std::to_string(allocs) + " allocation(s) while judging an oversized state");
  o.note("largest factory state " + std::to_string(biggest) + " bytes; worst case " + std::to_string(ws.size()) + " bytes; module budget " + std::to_string(own) + " bytes");
  o.summary = std::to_string(c.presets.size()) + " presets and a worst case under " + std::to_string(cap) + " bytes; a " + std::to_string(big.size()) + "-byte state refused";
  return o;
}

// A20 Live-state exclusion: saved during a ringing tail, a ramp in flight and a latched clip, the
// state equals one saved at rest with the same parameters.
Out rowA20(const Ctx& c) {
  Out o{"A20"};
  const double sr = 48000;
  auto a = session(c, sr);
  Buf hot = noise(512, 81);
  hot.L[100] = 3;
  render(*a, hot, 256);   // latches a clip
  applyTo(*a, moveAll(*a));
  render(*a, Buf(64), 64);   // a ramp in flight, the tail ringing
  const auto mt = a->meters();
  const std::string live = save(*a);
  auto b = fresh(c, targets(*a), sr, 512);
  render(*b, Buf(secs(1, sr)), 512);
  const std::string rest = save(*b);
  if (live != rest) o.fail("live", "a state saved mid-tail with a ramp in flight and a latched clip differs from one saved at rest");
  o.summary = std::string("saved mid-tail, mid-ramp, clip ") + (mt.inClip || mt.outClip ? "latched" : "NOT latched (the probe did not reach it)");
  if (!(mt.inClip || mt.outClip)) o.fail("probe", "the probe did not latch a clip, so the latch half proves nothing");
  return o;
}

// A21 Morph position: only for a module that declares it.
Out rowA21(const Ctx& c) {
  Out o{"A21"};
  if (!c.make()->declared().morphPosition) { o.status = "NA"; o.summary = "morphPosition not declared (RS-4.10 is optional)"; return o; }
  o.status = "NOTRUN";
  o.summary = "the module declares morphPosition; this harness has no morphWeights entry yet";
  return o;
}

// ================================================================================================
// Reported, never judged (A10's note; contract §2.2 layering: CPU is Layer-E): the time of the
// first block after a reset() whose parameters moved, which owes a decay design, beside a steady
// block. Only the morph-class keys move: a flip (for Scape, the Vintage engine) changes which
// engine sounds and so which design is owed, and would mix two different costs in one figure.
// Wall-clock reads are a TOOL's here, never the core's.
void timing(const Ctx& c) {
  const double sr = 44100;
  const std::uint32_t B = 128;
  auto m = fresh(c, {}, sr, B);
  const Buf in = noise(B, 91);
  Buf out(B);
  for (int i = 0; i < 400; ++i) m->process(in.L.data(), in.R.data(), out.L.data(), out.R.data(), B);
  using clk = std::chrono::steady_clock;
  std::vector<double> steady, owed;
  for (int i = 0; i < 400; ++i) {
    const auto t = clk::now();
    m->process(in.L.data(), in.R.data(), out.L.data(), out.R.data(), B);
    steady.push_back(std::chrono::duration<double, std::milli>(clk::now() - t).count());
  }
  Settings morphMoves;
  for (int rep = 0; rep < 21; ++rep) {
    morphMoves.clear();
    for (const auto& kv : moveAll(*m))
      for (const auto& p : m->manifest())
        if (p.address == kv.first && p.cls == Cls::Morph) morphMoves.push_back(kv);
    applyTo(*m, morphMoves);
    m->reset();
    const auto t = clk::now();
    m->process(in.L.data(), in.R.data(), out.L.data(), out.R.data(), B);
    owed.push_back(std::chrono::duration<double, std::milli>(clk::now() - t).count());
    for (int i = 0; i < 50; ++i) m->process(in.L.data(), in.R.data(), out.L.data(), out.R.data(), B);
  }
  std::sort(steady.begin(), steady.end());
  std::sort(owed.begin(), owed.end());
  std::printf("NOTE A10 :: REPORTED (Layer-E, not judged): the first %u-sample block after a reset() whose parameters moved: min %s ms, median %s ms, max %s ms over %zu; "
              "a steady block: median %s ms; the block lasts %s ms at 44.1k\n",
              B, num(owed.front()).c_str(), num(owed[owed.size() / 2]).c_str(), num(owed.back()).c_str(), owed.size(), num(steady[steady.size() / 2]).c_str(), num(1000.0 * B / sr).c_str());
}

// ================================================================================================
// A10 under RealtimeSanitizer: a scripted session with every audio-thread call inside the realtime
// scope and every main-thread call (prepare, staging, saving, parsing) outside it.
int rtsan(const Ctx& c, const std::string& mode) {
#ifndef ADMISSION_RTSAN
  (void)c;
  std::fprintf(stderr, "admission_harness: --rtsan %s needs the RealtimeSanitizer build\n", mode.c_str());
  return 2;
#else
  const double sr = 48000;
  const std::uint32_t B = 256;
  if (mode == "control") {
    __rtsan_realtime_enter();
    __rtsan_realtime_exit();
    std::printf("rtsan: control done\n");
    return 0;
  }
  auto m = fresh(c, {}, sr, B);
  const Buf in = noise(B, 101), z(B);
  Buf nf = noise(B, 102);
  nf.L[5] = std::nan("");
  nf.R[9] = INFINITY;
  Buf out(B);
  long calls = 0;
  auto block = [&](const Buf& x) {   // inside the scope: the caller entered it
    m->process(x.L.data(), x.R.data(), out.L.data(), out.R.data(), B);
    ++calls;
  };
  // prepared outside the scope: handles, values, patches, states
  const auto spec = m->manifest();
  std::vector<int> hs;
  for (const auto& s : spec) hs.push_back(m->resolve(s.address));
  std::vector<Patch> patches;
  for (const auto& [f, t] : c.presets) { Patch p; if (m->parsePatch(t, p, nullptr)) patches.push_back(p); }
  // 1. sweeps of every key, eight values each, one per block (macro moves reach a module as
  //    exactly these setParam calls: horde resolves macros, SPEC-MODULE-MACROS §5)
  for (std::size_t k = 0; k < spec.size(); ++k)
    for (int i = 0; i < 8; ++i) {
      const double v = spec[k].min + (spec[k].max - spec[k].min) * i / 7.0;
      __rtsan_realtime_enter();
      m->setParam(hs[k], v);
      block(in);
      __rtsan_realtime_exit();
    }
  // 2. many keys moved inside one block, as a macro bound to several parameters does
  {
    const Settings mv = moveAll(*m);
    std::vector<std::pair<int, double>> hv;
    for (const auto& [a, v] : mv) hv.emplace_back(m->resolve(a), v);
    __rtsan_realtime_enter();
    for (const auto& [h, v] : hv) m->setParam(h, v);
    for (int i = 0; i < 40; ++i) block(in);
    __rtsan_realtime_exit();
  }
  // 3. corner flips: each preset staged on the main thread, adopted on the audio thread
  for (const auto& p : patches) {
    m->stagePatch(p);
    __rtsan_realtime_enter();
    m->adoptStaged();
    for (int i = 0; i < 4; ++i) block(in);
    __rtsan_realtime_exit();
  }
  // 4. state loads
  const std::string st = save(*m);
  {
    const Settings mv = moveAll(*m);
    applyTo(*m, mv);   // outside: a different state to come back from
    m->stageState(st);
    __rtsan_realtime_enter();
    m->adoptStaged();
    for (int i = 0; i < 20; ++i) block(in);
    __rtsan_realtime_exit();
  }
  // 5. a reset during a move (the design it owes runs in the next block)
  {
    const Settings mv = moveAll(*m);
    std::vector<std::pair<int, double>> hv;
    for (const auto& [a, v] : mv) hv.emplace_back(m->resolve(a), v);
    __rtsan_realtime_enter();
    for (const auto& [h, v] : hv) m->setParam(h, v);
    block(in);
    m->reset();
    for (int i = 0; i < 20; ++i) block(in);
    __rtsan_realtime_exit();
  }
  // 6. sleep and wake: ring out, stop calling, change parameters through the load entry and by
  //    setParam while asleep, then resume
  {
    __rtsan_realtime_enter();
    for (int i = 0; i < 400; ++i) block(z);
    __rtsan_realtime_exit();
    m->stageState(st);
    __rtsan_realtime_enter();
    m->adoptStaged();
    m->setParam(hs[0], spec[0].max);
    for (int i = 0; i < 20; ++i) block(in);
    __rtsan_realtime_exit();
  }
  // 7. non-finite input, meters and latches read and cleared
  __rtsan_realtime_enter();
  block(nf);
  for (int i = 0; i < 20; ++i) block(in);
  volatile double sinkM = m->meters().inPeak + static_cast<double>(m->nonFiniteCount());
  (void)sinkM;
  m->clearMeters();
  m->clearNonFiniteLatch();
  __rtsan_realtime_exit();
  std::printf("rtsan: session done (%ld process calls inside the scope, %zu keys swept, %zu corner flips)\n", calls, spec.size(), patches.size());
  return 0;
#endif
}

using RowFn = Out (*)(const Ctx&);
const std::vector<std::pair<std::string, RowFn>>& rows() {
  static const std::vector<std::pair<std::string, RowFn>> k = {
      {"A1", rowA1}, {"A2", rowA2}, {"A3", rowA3}, {"A4", rowA4}, {"A5", rowA5}, {"A6", rowA6}, {"A8", rowA8}, {"A9", rowA9},
      {"A11", rowA11}, {"A12", rowA12}, {"A15", rowA15}, {"A17", rowA17}, {"A18", rowA18}, {"A19", rowA19}, {"A20", rowA20}, {"A21", rowA21}};
  return k;
}

}  // namespace

int main(int argc, char** argv) {
  std::string module, tree, plant, rtsanMode;
  std::set<std::string> want;
  bool readings = true, manifest = false, doTiming = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto val = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--module") module = val();
    else if (a == "--tree") tree = val();
    else if (a == "--plant") plant = val();
    else if (a == "--rtsan") rtsanMode = val();
    else if (a == "--no-readings") readings = false;
    else if (a == "--manifest-json") manifest = true;
    else if (a == "--timing") doTiming = true;
    else if (a == "--list-plants") {
      for (const auto& p : plants()) std::printf("PLANT %s %s :: %s\n", p.id, p.row, p.what);
      return 0;
    } else if (a == "--rows") {
      std::string v = val(), cur;
      for (char ch : v + ",") { if (ch == ',') { if (!cur.empty()) want.insert(cur); cur.clear(); } else cur += ch; }
    } else { std::fprintf(stderr, "admission_harness: unknown argument %s\n", a.c_str()); return 2; }
  }
  ModuleInfo info;
  if (module == "scape") info = scapeModule(tree);
  else { std::fprintf(stderr, "admission_harness: unknown module '%s' (known: scape)\n", module.c_str()); return 2; }
  Ctx c;
  c.make = info.make;
  c.presets = info.presets;
  c.readings = readings;
  if (!plant.empty()) {
    const PlantDef* d = nullptr;
    for (const auto& p : plants())
      if (plant == p.id) d = &p;
    if (!d) { std::fprintf(stderr, "admission_harness: unknown plant '%s'\n", plant.c_str()); return 2; }
    auto base = info.make;
    auto wrap = d->wrap;
    c.make = [base, wrap] { return wrap(base()); };
  }
  if (manifest) { std::fputs(c.make()->manifestJson().c_str(), stdout); return 0; }
  if (!rtsanMode.empty()) return rtsan(c, rtsanMode);
  std::printf("MODULE %s :: %zu factory presets%s\n", info.name.c_str(), c.presets.size(), plant.empty() ? "" : (", plant " + plant).c_str());
  if (doTiming) { timing(c); return 0; }
  for (const auto& [id, fn] : rows())
    if (want.empty() || want.count(id)) fn(c).print();
  return 0;
}
