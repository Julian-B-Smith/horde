// passthrough_adapter.cpp — horde's do-nothing test module behind admission::Module (ROADMAP B714, acceptance (1)).
//
// WHY IT EXISTS. "A module is hosted from its pin, manifest and state-budget row, with no per-module branch" is proven
// by behaviour, not by a name search (critic finding 3): this module arrives by data alone (a modules.json entry, its
// manifest, a budget row, a ledger block, an overlay and this adapter) and the test configuration hosts it with no
// change to preview_plugin.cpp or bridge_core.*; it is published, saved, restored and passes the generated checks.
// It is never linked into the plug-in (modules.json test_only).
//
// What it does: out = in x gain, gain from pass.gain in dB (unity at its default, 0 dB). pass.mode is a flip key that
// changes nothing (declared `silent`, and true to it), so the slot's flip handling has a key to carry. No tail, no
// latency, any block size.
#include "module.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace admission {
namespace {

constexpr int kGain = 0, kMode = 1, kCount = 2;
constexpr const char* kAddress[kCount] = {"pass.gain", "pass.mode"};
constexpr double kMin[kCount] = {-12, 0}, kMax[kCount] = {12, 1}, kDefault[kCount] = {0, 0};

class PassModule final : public Module {
 public:
  PassModule() {
    for (int k = 0; k < kCount; ++k) value_[k] = staged_[k] = kDefault[k];
  }
  std::vector<ParamSpec> manifest() const override {
    std::vector<ParamSpec> out(kCount);
    for (int k = 0; k < kCount; ++k) {
      out[std::size_t(k)].address = kAddress[k];
      out[std::size_t(k)].unitDeclared = true;
      out[std::size_t(k)].unit = k == kGain ? "dB" : "";
      out[std::size_t(k)].cls = k == kGain ? Cls::Morph : Cls::Flip;
      out[std::size_t(k)].rateDeclared = true;
      out[std::size_t(k)].min = kMin[k];
      out[std::size_t(k)].max = kMax[k];
      out[std::size_t(k)].def = kDefault[k];
      out[std::size_t(k)].steps = k == kGain ? 0 : 2;
      if (k == kMode) out[std::size_t(k)].sw = Switch::Silent, out[std::size_t(k)].switchSource = "its manifest's switch.method \"silent\"";
    }
    return out;
  }
  std::string manifestJson() const override { return ""; }
  Declared declared() const override { return {false, true, false}; }
  bool prepare(double, std::uint32_t) override { prepared_ = true; return true; }
  void reset() override {}
  foundations::OperatorProperties properties() const override {
    foundations::OperatorProperties p;
    if (!prepared_) return p;
    p.latency = foundations::LatencyDecl{0, 0.0, foundations::ResidualSign::kNone};
    p.tail = foundations::TailDecl{0, false};
    p.min_block = 1;
    return p;
  }
  int resolve(std::string_view address) const override {
    for (int k = 0; k < kCount; ++k)
      if (address == kAddress[k]) return k;
    return -1;
  }
  bool setParam(int h, double v) override {
    if (h < 0 || h >= kCount || !std::isfinite(v)) return false;
    value_[h] = std::clamp(v, kMin[h], kMax[h]);
    return true;
  }
  double getParam(int h) const override { return h >= 0 && h < kCount ? value_[h] : std::numeric_limits<double>::quiet_NaN(); }
  void process(const Sample* inL, const Sample* inR, Sample* outL, Sample* outR, std::uint32_t n) override {
    const double g = std::pow(10.0, value_[kGain] / 20.0);   // 0 dB is 1 exactly: the default passes its input bit for bit
    for (std::uint32_t i = 0; i < n; ++i) outL[i] = inL[i] * g, outR[i] = inR[i] * g;
  }
  std::size_t stateBudget() const override { return 1024; }
  std::size_t saveState(char* out, std::size_t cap) const override {
    char text[128];
    const int len = std::snprintf(text, sizeof text, "%s %.17g\n%s %.17g\n", kAddress[0], value_[0], kAddress[1], value_[1]);
    if (len < 0 || std::size_t(len) > cap) return 0;
    std::memcpy(out, text, std::size_t(len));
    return std::size_t(len);
  }
  bool stageState(std::string_view text) override {
    std::istringstream in{std::string(text)};
    std::string a;
    double x = 0;
    while (in >> a >> x) {
      const int k = resolve(a);
      if (k < 0 || !std::isfinite(x)) return false;
      staged_[k] = std::clamp(x, kMin[k], kMax[k]);
    }
    return true;
  }
  bool adoptStaged() override {
    std::copy(staged_, staged_ + kCount, value_);
    return true;
  }
  bool parsePatch(std::string_view, Patch& out, std::string*) const override { out = Patch{}; return true; }
  std::string writePatch(const Patch&) const override { return ""; }
  bool stagePatch(const Patch&) override { return true; }
  double macroValue(const Bind& b, double v) const override { return b.lo + (b.hi - b.lo) * std::clamp(v, 0.0, 1.0); }
  Meters meters() const override { return {}; }
  void clearMeters() override {}
  std::uint64_t nonFiniteCount() const override { return 0; }
  bool nonFiniteLatched() const override { return false; }
  void clearNonFiniteLatch() override {}
  bool plantNonFiniteState() override { return false; }
  Settings dryProbe() const override { return {{"pass.gain", 0}}; }
  bool dryExact() const override { return true; }
  Settings stressProbe() const override { return {{"pass.gain", -6}, {"pass.mode", 1}}; }

 private:
  double value_[kCount] = {}, staged_[kCount] = {};
  bool prepared_ = false;
};

}  // namespace

ModuleInfo passthroughModule(const std::string&) {
  ModuleInfo info;
  info.name = "Pass";
  info.make = [] { return std::make_unique<PassModule>(); };
  return info;
}

}  // namespace admission
