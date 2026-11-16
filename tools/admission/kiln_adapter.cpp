// kiln_adapter.cpp — Kiln's two modules (kiln::Eq, kiln::DriveModule) behind admission::Module
// (ROADMAP B605, ADR-351). Everything Kiln-specific the admission rows need lives here and
// nowhere else (ADR-295): the handle type, the manifest's mapping onto contract §4.1, the
// declared switch methods, the test seam for row A9 and the probe settings.
//
// ONE FILE FOR BOTH MODULES, because Kiln's two modules implement one interface (kiln/module.h,
// written to this harness's own shape) and differ only in four facts: their name, their probes,
// whether the dry probe is exact, and their A9 seam. Those four are a Traits struct each; the
// rest is written once.
//
// Kiln's own reference adapters (integrations/kiln/reference-adapters/*.cpp.txt) were read as
// reference only. Two differences from them, on purpose: an enum crosses the boundary by an
// explicit switch, never an int cast (a renumbering on either side then fails to compile or reads
// Undeclared, instead of silently meaning something else), and a flip key's transition length is
// its declared `switch.length_s`, not the morph smoothing time.
//
// Built against a Kiln tree that tools/admission_check.py locates ($HORDE_KILN_DIR); kiln_core is
// built by Kiln's own CMake (tools/admission/CMakeLists.txt), so its arithmetic flags are Kiln's
// own (-ffp-contract=off, Kiln D-007). Nothing here, and nothing the build writes, touches that
// tree.
#include "module.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "kiln/drive.h"
#include "kiln/eq.h"
#include "kiln/manifest.h"
#include "kiln/patch.h"

namespace kiln {
// Kiln's own test seam: DriveModule declares `friend struct DriveTestAccess` for its tests to
// plant faults in internal state, and its tests define the struct. Kiln's tests are not linked
// here, so this is the one definition in this binary (Scape's ReverbTestAccess, in kind). One NaN
// in the DC blocker's output memory of the left channel: it is read back on the next sample, in
// the recursion the guard has to catch.
struct DriveTestAccess {
  static void plant(DriveModule& m) { m.dcY1_[0] = std::numeric_limits<double>::quiet_NaN(); }
};
}  // namespace kiln

namespace admission {
namespace {

Cls classOf(kiln::ParamClass c) {
  switch (c) {
    case kiln::ParamClass::Morph: return Cls::Morph;
    case kiln::ParamClass::Flip: return Cls::Flip;
    case kiln::ParamClass::Device: return Cls::Device;
    case kiln::ParamClass::Undeclared: break;
  }
  return Cls::None;
}

// THE SWITCH METHODS. Kiln states them in its manifest (`switch` object, ADR-309), and the
// adapter reads that declaration, never a transcription of prose. "crossfade" maps to the
// contract's equal-gain crossfade only with law "equal-gain": §12.1 admits no other law, so any
// other law reads Undeclared and A11 refuses it.
Switch switchOf(const kiln::SwitchDecl& d) {
  switch (d.method) {
    case kiln::SwitchMethod::Crossfade: return d.law == "equal-gain" ? Switch::EqualGainCrossfade : Switch::Undeclared;
    case kiln::SwitchMethod::Gate: return Switch::Gate;
    case kiln::SwitchMethod::SeededJump: return Switch::SeededJump;
    case kiln::SwitchMethod::Silent: return Switch::Silent;
    case kiln::SwitchMethod::Undeclared: break;
  }
  return Switch::Undeclared;
}
const char* methodName(kiln::SwitchMethod m) {
  switch (m) {
    case kiln::SwitchMethod::Crossfade: return "crossfade";
    case kiln::SwitchMethod::Gate: return "gate";
    case kiln::SwitchMethod::SeededJump: return "seeded-jump";
    case kiln::SwitchMethod::Silent: return "silent";
    case kiln::SwitchMethod::Undeclared: break;
  }
  return "undeclared";
}

kiln::Curve curveOf(const std::string& s) { return s == "exp" ? kiln::Curve::Exp : s == "log" ? kiln::Curve::Log : kiln::Curve::Lin; }
std::string curveName(kiln::Curve c) { return c == kiln::Curve::Exp ? "exp" : c == kiln::Curve::Log ? "log" : "lin"; }

kiln::Patch toKiln(const Patch& p) {
  kiln::Patch k;
  k.name = p.name;
  k.values = p.values;
  for (const auto& m : p.macros) {
    kiln::Macro km;
    km.label = m.label;
    km.home = m.home;
    for (const auto& b : m.bind) km.bind.push_back({b.address, b.lo, b.hi, curveOf(b.curve)});
    k.macros.push_back(std::move(km));
  }
  return k;
}
Patch fromKiln(const kiln::Patch& k) {
  Patch p;
  p.name = k.name;
  p.values = k.values;
  for (const auto& m : k.macros) {
    Macro am;
    am.label = m.label;
    am.home = m.home;
    for (const auto& b : m.bind) am.bind.push_back({b.address, b.lo, b.hi, curveName(b.curve)});
    p.macros.push_back(std::move(am));
  }
  return p;
}

foundations::ResidualSign signOf(kiln::ResidualSign s) {
  switch (s) {
    case kiln::ResidualSign::kNone: return foundations::ResidualSign::kNone;
    case kiln::ResidualSign::kEarly: return foundations::ResidualSign::kEarly;
    case kiln::ResidualSign::kLate: return foundations::ResidualSign::kLate;
  }
  return foundations::ResidualSign::kNone;
}
foundations::OperatorProperties propertiesOf(const kiln::OperatorProperties& o) {
  foundations::OperatorProperties f;
  if (o.latency) f.latency = foundations::LatencyDecl{o.latency->samples, o.latency->residual, signOf(o.latency->sign)};
  if (o.tail) f.tail = foundations::TailDecl{o.tail->samples, o.tail->may_change};
  f.min_block = o.min_block;
  if (o.load) f.load = foundations::LoadDecl{o.load->transition_samples};
  return f;
}

Settings settingsOf(const std::vector<std::pair<std::string_view, double>>& v) {
  Settings s;
  for (const auto& [k, x] : v) s.emplace_back(std::string(k), x);
  return s;
}

// ---- what differs between the two modules --------------------------------------------------

struct EqTraits {
  using Mod = kiln::Eq;
  static constexpr const char* kName = "eq";
  static Settings dryProbe() { return settingsOf(kiln::eq::dryProbe()); }
  static Settings stressProbe() { return settingsOf(kiln::eq::stressProbe()); }
  // Every band off is a skipped band, not a flat filter, so out = in x io.inGain x io.outGain
  // exactly (eq.h: "EXACT BYPASS"): A4 and A6 read it sample for sample.
  static constexpr bool kDryExact = true;
  // False when no band runs: A9 then reports that half NOT RUN (module.h's contract for the seam).
  static bool plant(Mod& m) { return kiln::plantNonFiniteState(m); }
};

struct DriveTraits {
  using Mod = kiln::DriveModule;
  static constexpr const char* kName = "drive";
  // mix 0 is the nearest the drive comes to dry, and it is not exact: the input goes through the
  // up-sampler, the averaging and the down-sampler, a unity-gain allpass-like response with a
  // 3.5031-sample delay (drive.h, THE MIX). A4 reads the delay there; A6 judges against the
  // module's own reference renders (ADR-351).
  static Settings dryProbe() { return {{"drive.mix", 0}}; }
  // Every stage engaged: 18 dB into the hard curve, a half shape, the full wet mix, auto-gain on
  // (Kiln's own stress probe for this module, from its reference adapter).
  static Settings stressProbe() { return {{"drive.amount", 18}, {"drive.shape", 0.5}, {"drive.mix", 1}, {"drive.autogain", 1}, {"drive.curve", 1}}; }
  static constexpr bool kDryExact = false;
  static bool plant(Mod& m) { kiln::DriveTestAccess::plant(m); return true; }
};

template <class T>
class KilnAdapter final : public Module {
 public:
  std::vector<ParamSpec> manifest() const override {
    std::vector<ParamSpec> v;
    for (const auto& s : m_.manifest()) {
      ParamSpec p;
      p.address = std::string(s.key);
      p.unit = s.unit ? std::string(*s.unit) : "";
      p.unitDeclared = s.unit.has_value();
      p.cls = classOf(s.cls);
      p.rateDeclared = s.rate != kiln::RateClass::Undeclared;
      p.min = s.min;
      p.max = s.max;
      p.def = s.def;
      p.steps = s.steps;
      p.transitionS = p.cls == Cls::Flip ? s.sw.length_s : s.transition_s;
      if (p.cls == Cls::Flip) {
        p.sw = switchOf(s.sw);
        p.switchSource = std::string("Kiln manifest switch.method \"") + methodName(s.sw.method) + "\" law \"" + std::string(s.sw.law) + "\"";
      }
      v.push_back(std::move(p));
    }
    return v;
  }
  // As it ships: the one writer, kiln::manifestJson (module.h's header: not a virtual).
  std::string manifestJson() const override { return kiln::manifestJson(T::kName, m_.declared(), m_.manifest()); }
  Declared declared() const override {
    const auto d = m_.declared();
    return {d.changes_image, d.changes_level, false};   // neither Kiln module has a morph position
  }
  bool prepare(double sr, std::uint32_t mb) override { return m_.prepare(sr, mb); }
  void reset() override { m_.reset(); }
  foundations::OperatorProperties properties() const override { return propertiesOf(m_.properties()); }
  int resolve(std::string_view a) const override {
    const auto h = m_.resolve(a);
    return h ? h->row : -1;
  }
  bool setParam(int h, double v) override { return m_.setParam(kiln::ParamHandle{h}, v); }
  double getParam(int h) const override { return m_.getParam(kiln::ParamHandle{h}); }
  void process(const Sample* a, const Sample* b, Sample* c, Sample* d, std::uint32_t n) override { m_.process(a, b, c, d, n); }
  std::size_t stateBudget() const override { return m_.stateBudget(); }
  std::size_t saveState(char* o, std::size_t cap) const override { return m_.saveState(o, cap); }
  bool stageState(std::string_view t) override { return m_.stageState(t); }
  bool adoptStaged() override { return m_.adoptStaged(); }
  bool parsePatch(std::string_view t, Patch& o, std::string* e) const override {
    kiln::Patch k;
    if (!m_.parsePatch(t, k, e)) return false;
    o = fromKiln(k);
    return true;
  }
  std::string writePatch(const Patch& p) const override { return m_.writePatch(toKiln(p)); }
  bool stagePatch(const Patch& p) override { return m_.stagePatch(toKiln(p)); }
  double macroValue(const Bind& b, double v) const override { return kiln::macroValue(kiln::Bind{b.address, b.lo, b.hi, curveOf(b.curve)}, v); }
  Meters meters() const override {
    const auto x = m_.meters();
    return {x.inPeak, x.outPeak, x.inClip, x.outClip};
  }
  void clearMeters() override { m_.clearMeters(); }
  std::uint64_t nonFiniteCount() const override { return m_.nonFiniteCount(); }
  bool nonFiniteLatched() const override { return m_.nonFiniteLatched(); }
  void clearNonFiniteLatch() override { m_.clearNonFiniteLatch(); }
  bool plantNonFiniteState() override { return T::plant(m_); }
  Settings dryProbe() const override { return T::dryProbe(); }
  Settings stressProbe() const override { return T::stressProbe(); }
  bool dryExact() const override { return T::kDryExact; }

 private:
  typename T::Mod m_;
};

// The factory presets of one module: every regular, non-dot file under presets/<module>/factory/
// (today one each, "Default"), sorted by name. A file that cannot be read is skipped, not an empty
// preset: the row then judges the presets that exist.
ModuleInfo infoFor(const std::string& tree, const char* label, const char* dir, std::function<std::unique_ptr<Module>()> make) {
  ModuleInfo info;
  info.name = label;
  info.make = std::move(make);
  namespace fs = std::filesystem;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(fs::path(tree) / "presets" / dir / "factory", ec)) {
    if (!e.is_regular_file() || e.path().filename().string().front() == '.') continue;
    std::ifstream f(e.path(), std::ios::binary);
    if (!f) continue;
    std::stringstream ss;
    ss << f.rdbuf();
    info.presets.emplace_back(e.path().filename().string(), ss.str());
  }
  std::sort(info.presets.begin(), info.presets.end());
  return info;
}

}  // namespace

ModuleInfo kilnEqModule(const std::string& tree) {
  return infoFor(tree, "Kiln EQ", "eq", [] { return std::make_unique<KilnAdapter<EqTraits>>(); });
}
ModuleInfo kilnDriveModule(const std::string& tree) {
  return infoFor(tree, "Kiln drive", "drive", [] { return std::make_unique<KilnAdapter<DriveTraits>>(); });
}

}  // namespace admission
