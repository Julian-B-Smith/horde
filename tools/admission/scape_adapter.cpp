// scape_adapter.cpp — Scape's shim (scape/fx_module.h, scape/patch.h) behind admission::Module.
//
// Everything Scape-specific the admission rows need lives here and nowhere else (ADR-295): the
// handle type, the patch text, the manifest's mapping onto contract §4.1, the switch-method table
// Scape states only in prose, the test seam for row A9 and the probe settings. When Scape's shim
// is swapped for the FOUNDATIONS header, this file is the only one that changes.
//
// Built against a Scape tree that tools/admission_check.py locates ($HORDE_SCAPE_DIR); nothing
// here, and nothing the build writes, touches that tree.
#include "module.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "scape/fx_module.h"
#include "scape/patch.h"
#include "scape/reverb.h"

namespace scape {
// Scape's own test seam: Reverb declares `friend struct ReverbTestAccess` for its tests to plant
// faults in internal state, and its tests define the struct. Scape's tests are not linked here,
// so this is the one definition in this binary. It mirrors Scape's core/tests poison(): one NaN
// written where FDN line 3 is read back one line-delay later, in whichever engine is sounding.
struct ReverbTestAccess {
  static void poison(Reverb& v) {
    Reverb& e = v.eraCore_ && v.eraActive_ == 1 ? *v.eraCore_ : v;
    e.line_[3][static_cast<std::size_t>((e.lw_ + e.LBUF_ - 1) % e.LBUF_)] = std::numeric_limits<double>::quiet_NaN();
  }
};
}  // namespace scape

namespace admission {
namespace {

using scape::Param;

admission::Cls classOf(scape::ParamClass c) {
  switch (c) {
    case scape::ParamClass::Morph: return Cls::Morph;
    case scape::ParamClass::Flip: return Cls::Flip;
    case scape::ParamClass::Device: return Cls::Device;
  }
  return Cls::None;
}

// THE SWITCH METHODS (contract §12.1: "Every module: declare each flip-class key's switch method
// and length"). ADR-309 makes the manifest's `switch` object the declaration: `method` is one of
// crossfade | gate | seeded-jump | silent, with `length_s`, and `law` / `of` where they apply.
// The adapter reads Scape's ParamDecl::switchMethod, which is what Scape's manifest is written
// from, never a transcription of Scape's prose. That transcription is what kept A11 pinned after
// Scape's fix (11e1698). "crossfade" maps to the contract's equal-gain crossfade: §12.1 admits no
// other law, and A11's click reading judges it. A flip key with no method, or a word outside the
// four, reads Undeclared, and row A11 says so by name.
Switch switchOf(const char* method) {
  if (!method || !*method) return Switch::Undeclared;
  if (std::strcmp(method, "crossfade") == 0) return Switch::EqualGainCrossfade;
  if (std::strcmp(method, "gate") == 0) return Switch::Gate;
  if (std::strcmp(method, "seeded-jump") == 0) return Switch::SeededJump;
  if (std::strcmp(method, "silent") == 0) return Switch::Silent;
  return Switch::Undeclared;
}

int enumSteps(const scape::ParamDecl& d) {
  if (std::strcmp(d.unit, "enum") != 0) return 0;
  int n = 1;
  for (const char* c = d.values; *c; ++c) n += *c == '|';
  return n;
}

const char* curveName(scape::MacroCurve c) { return c == scape::MacroCurve::Lin ? "lin" : "exp"; }

bool paramOf(std::string_view address, Param& out) {
  for (int k = 0; k < scape::kParamCount; ++k)
    if (scape::FxModule::address(static_cast<Param>(k)) == address) { out = static_cast<Param>(k); return true; }
  return false;
}

Patch fromScape(const std::string& name, const scape::PatchBody& b) {
  Patch p;
  p.name = name;
  for (int k = 0; k < scape::kParamCount; ++k) {
    const Param q = static_cast<Param>(k);
    if (scape::paramDecl(q).cls == scape::ParamClass::Device) continue;   // never in a patch (Scape D-061)
    p.values.emplace_back(scape::FxModule::address(q), b.values[static_cast<std::size_t>(k)]);
  }
  for (const auto& m : b.macros) {
    Macro mm;
    mm.label = m.label;
    mm.home = m.home;
    for (const auto& x : m.bind) mm.bind.push_back({scape::FxModule::address(x.param), x.lo, x.hi, curveName(x.curve)});
    p.macros.push_back(std::move(mm));
  }
  return p;
}

// A neutral patch as Scape's body. Device addresses are dropped (a body holds them at defaults
// and never reads them); a `log` curve has no Scape value and is written as lin, then re-spelled
// in the text by writePatch below, so the parser — not this converter — is what refuses it.
scape::PatchBody toScape(const Patch& p, std::vector<std::size_t>* logBinds) {
  scape::PatchBody b;
  for (const auto& [addr, v] : p.values) {
    Param q;
    if (paramOf(addr, q) && scape::paramDecl(q).cls != scape::ParamClass::Device) b.values[static_cast<std::size_t>(q)] = v;
  }
  std::size_t ordinal = 0;
  for (const auto& m : p.macros) {
    scape::MacroSlot s;
    s.label = m.label;
    s.home = m.home;
    for (const auto& x : m.bind) {
      scape::MacroBinding sb;
      if (!paramOf(x.address, sb.param)) continue;
      sb.lo = x.lo;
      sb.hi = x.hi;
      sb.curve = x.curve == "exp" ? scape::MacroCurve::Exp : scape::MacroCurve::Lin;
      if (x.curve == "log" && logBinds) logBinds->push_back(ordinal);
      s.bind.push_back(sb);
      ++ordinal;
    }
    b.macros.push_back(std::move(s));
  }
  return b;
}

class ScapeModule final : public Module {
 public:
  ScapeModule() {
    for (int k = 0; k < scape::kParamCount; ++k) handles_.push_back(*mod_.resolve(scape::FxModule::address(static_cast<Param>(k))));
  }

  std::vector<ParamSpec> manifest() const override {
    std::vector<ParamSpec> out;
    for (int k = 0; k < scape::kParamCount; ++k) {
      const Param q = static_cast<Param>(k);
      const auto& i = scape::paramInfo(q);
      const auto& d = scape::paramDecl(q);
      ParamSpec s;
      s.address = scape::FxModule::address(q);
      s.unit = d.unit ? d.unit : "";
      s.unitDeclared = d.unit != nullptr;
      s.cls = classOf(d.cls);
      s.rateDeclared = true;   // ParamDecl::rate always carries a value (ReadCadence)
      s.min = i.min;
      s.max = i.max;
      s.def = i.def;
      s.transitionS = d.transitionSeconds;
      s.steps = enumSteps(d);
      if (s.cls == Cls::Flip) {
        s.sw = switchOf(d.switchMethod);
        s.switchSource = std::string("Scape manifest switch.method \"") + (d.switchMethod ? d.switchMethod : "") + "\"";
      }
      out.push_back(std::move(s));
    }
    return out;
  }
  std::string manifestJson() const override { return scape::manifestJson(true); }
  Declared declared() const override { return {scape::kChangesImage, scape::kChangesLevel, false}; }

  bool prepare(double sr, std::uint32_t maxBlock) override { return mod_.prepare(sr, maxBlock); }
  void reset() override { mod_.reset(); }
  foundations::OperatorProperties properties() const override { return mod_.properties(); }

  int resolve(std::string_view address) const override {
    const auto h = mod_.resolve(address);
    if (!h) return -1;
    for (std::size_t k = 0; k < handles_.size(); ++k)
      if (handles_[k] == *h) return static_cast<int>(k);
    return -1;
  }
  bool setParam(int h, double v) override { return valid(h) && mod_.setParam(handles_[static_cast<std::size_t>(h)], v); }
  double getParam(int h) const override {
    return valid(h) ? mod_.getParam(handles_[static_cast<std::size_t>(h)]) : std::numeric_limits<double>::quiet_NaN();
  }

  void process(const Sample* inL, const Sample* inR, Sample* outL, Sample* outR, std::uint32_t n) override {
    mod_.process(inL, inR, outL, outR, n);
  }

  std::size_t stateBudget() const override { return scape::FxModule::kMaxStateBytes; }
  std::size_t saveState(char* out, std::size_t cap) const override { return mod_.saveState(out, cap); }
  bool stageState(std::string_view text) override { return mod_.stageState(text); }
  bool adoptStaged() override { return mod_.adoptStaged(); }

  bool parsePatch(std::string_view text, Patch& out, std::string* error) const override {
    scape::Patch p;
    if (!scape::parsePatch(text, p, error)) return false;
    if (p.corners) {   // a four-corner file fills horde's corners (A13, a slot row); one patch here
      if (error) *error = "a four-corner file, not one patch";
      return false;
    }
    out = fromScape(p.name, p.single);
    return true;
  }
  std::string writePatch(const Patch& p) const override {
    std::vector<std::size_t> logBinds;
    scape::Patch sp;
    sp.name = p.name;
    sp.single = toScape(p, &logBinds);
    std::string text = scape::writePatch(sp);
    if (logBinds.empty()) return text;
    // Re-spell the curve of each `log` binding: Scape's format puts it last on its `bind` line.
    std::istringstream in(text);
    std::string line, out;
    std::size_t ordinal = 0;
    while (std::getline(in, line)) {
      if (line.rfind("bind ", 0) == 0) {
        if (std::find(logBinds.begin(), logBinds.end(), ordinal) != logBinds.end()) line = line.substr(0, line.rfind(' ')) + " log";
        ++ordinal;
      }
      out += line + "\n";
    }
    return out;
  }
  bool stagePatch(const Patch& p) override { return mod_.stagePatch(toScape(p, nullptr)); }
  double macroValue(const Bind& b, double v) const override {
    scape::MacroBinding sb;
    if (!paramOf(b.address, sb.param) || b.curve == "log") return std::numeric_limits<double>::quiet_NaN();
    sb.lo = b.lo;
    sb.hi = b.hi;
    sb.curve = b.curve == "exp" ? scape::MacroCurve::Exp : scape::MacroCurve::Lin;
    return scape::macroValue(sb, v);
  }

  Meters meters() const override {
    const scape::Reverb* r = mod_.engine();
    if (!r) return {};
    return {r->inPeak(), r->outPeak(), r->inClipped(), r->outClipped()};
  }
  void clearMeters() override { mod_.clearMeters(); }
  std::uint64_t nonFiniteCount() const override { return mod_.engine() ? mod_.engine()->nonFiniteCount() : 0; }
  bool nonFiniteLatched() const override { return mod_.engine() && mod_.engine()->nonFiniteLatched(); }
  void clearNonFiniteLatch() override { mod_.clearNonFiniteLatch(); }

  bool plantNonFiniteState() override {
    // engine() is const because nothing but a test may write through it; this is that test.
    auto* r = const_cast<scape::Reverb*>(mod_.engine());
    if (!r) return false;
    scape::ReverbTestAccess::poison(*r);
    return true;
  }
  // mix 0: out = (dry * 1 + wet * 0) * outGain, the dry already through the input gain
  // (reverb.cpp's output line), so the module passes its input times the two gains exactly.
  Settings dryProbe() const override { return {{"verb.mix", 0}}; }
  // Every stage sounding: a wetter mix, both early reflections and tail, the spin and the
  // modulation on, a set mixing time, a leaning image. Clean era: the flip rows add Vintage.
  Settings stressProbe() const override {
    return {{"verb.mix", 0.6}, {"verb.erSend", 1.0}, {"verb.spinDepth", 1.5}, {"verb.modDepth", 0.8},
            {"verb.mixTimeMs", 120}, {"verb.image", 0.8}, {"verb.decay", 4.0}, {"verb.size", 0.8}};
  }

 private:
  bool valid(int h) const { return h >= 0 && static_cast<std::size_t>(h) < handles_.size(); }
  scape::FxModule mod_;
  std::vector<scape::ParamHandle> handles_;
};

}  // namespace

ModuleInfo scapeModule(const std::string& tree) {
  ModuleInfo info;
  info.name = "Scape";
  info.make = [] { return std::make_unique<ScapeModule>(); };
  namespace fs = std::filesystem;
  const fs::path dir = fs::path(tree) / "presets" / "factory";
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(dir, ec)) {
    if (e.path().extension() != ".scape-patch") continue;
    std::ifstream f(e.path(), std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    info.presets.emplace_back(e.path().filename().string(), ss.str());
  }
  std::sort(info.presets.begin(), info.presets.end());
  return info;
}

}  // namespace admission
