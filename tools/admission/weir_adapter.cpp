// weir_adapter.cpp — Weir's SVF 12 (weir/svf.h) behind admission::Module, the shape of scape_adapter.cpp
// (ROADMAP B714, ADR-410 r4: "an adapter is needed only where it lacks the interface").
//
// WEIR SHIPS NO FX OPERATOR. Its manifest (docs/fx/manifest-fxfilter.json at weir.pin, ratified Weir D-017, budget
// D-020) declares the keys; the operator around the filter is HORDE'S, authored here, and every reading below is
// HORDE-INTERIM until Weir confirms it or ships its own operator (the lead's notice to Weir names each one):
//   - fxfilter.mix is a dry/wet of the filter's output against its input, out = in x (1 - mix) + filtered x mix, in
//     double, written exactly as the pre-B714 post_filter.h wrote it so a migrated patch plays the same bits. It is
//     applied per block, unsmoothed, as before: the manifest's transition_s 0.005 for mix is not honoured yet.
//   - fxfilter.mode is the index 0..4 of the manifest's values, and Weir's own Params::mode is that + 1 (svf.h:78-79:
//     0 is Weir's bypass, which the slot's on switch covers).
//   - fxfilter.type has one value (SVF 12) and is a constant the slot never exposes; it is accepted and ignored.
//   - Latency 0 (svf.h: latency() 0). min_block 1 (Weir's Q-005: bit-identical for any block split). No load entry.
//   - TAIL: kInfiniteTail while Weir's own tail() bound (every future output, by the state's norm) is at or above
//     kRungOutThreshold, 0 once it is below, may_change. That is the pre-B714 shell's stop (post_filter.h rung_out:
//     under 1e-7, below -140 dB, inaudible), restored by the human's ruling (ADR-413, "Restore the -140 dB stop"):
//     once it is declared over, the rack's idle cut makes the output exact zero. Holding Weir to exact zero instead kept
//     it, and a reverb after it, running on an idle instance about 96 s after a note (measured; main: about 27 s).
//   - The state text: one line per key, "<address> <value %.17g>", in manifest order; stateBudget is the manifest's.
//   - Weir's own smoothing ramp is ON (5 ms, as post_filter.h had it): nothing upstream glides these keys.
// Weir declares no io keys, so the slot supplies the pair (RS-4.7); this adapter has none.
//
// Built against a Weir tree at weir.pin that tools/rack/modules/weir.cmake checks; nothing here touches that tree.
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

#include "weir/svf.h"

namespace admission {
namespace {

// Weir's tail is over once its tail() bound is below this (-140 dB): HORDE-INTERIM, per ADR-413.
constexpr double kRungOutThreshold = 1e-7;

// The manifest's keys at the pin, in its order. A transcription of docs/fx/manifest-fxfilter.json for this adapter's
// manifest() (admission's A1); the rack never reads it, it reads the pinned manifest itself.
struct Key { const char* address; Cls cls; double min, max, def; const char* unit; int steps; double transition; };
constexpr Key kKeys[] = {
    {"fxfilter.type", Cls::Flip, 0, 0, 0, "", 1, 0.005},
    {"fxfilter.mode", Cls::Flip, 0, 4, 0, "", 5, 0.005},
    {"fxfilter.cutoff", Cls::Morph, 20, 20000, 1000, "Hz", 0, 0.005},
    {"fxfilter.resonance", Cls::Morph, 0, 1, 0, "", 0, 0.005},
    {"fxfilter.morph", Cls::Morph, 0, 1, 0, "", 0, 0.005},
    {"fxfilter.mix", Cls::Morph, 0, 1, 1, "", 0, 0.005},
};
constexpr int kCount = int(sizeof kKeys / sizeof kKeys[0]);
enum : int { kType = 0, kMode, kCutoff, kResonance, kMorph, kMix };
constexpr std::size_t kStateBudget = 4096;   // the manifest's stateBudget (Weir D-020)
constexpr double kSmoothingSeconds = 0.005;

class WeirModule final : public Module {
 public:
  WeirModule() {
    for (int k = 0; k < kCount; ++k) value_[k] = staged_[k] = kKeys[k].def;
  }

  std::vector<ParamSpec> manifest() const override {
    std::vector<ParamSpec> out;
    for (const Key& k : kKeys) {
      ParamSpec s;
      s.address = k.address;
      s.unit = k.unit;
      s.unitDeclared = true;
      s.cls = k.cls;
      s.rateDeclared = true;
      s.min = k.min;
      s.max = k.max;
      s.def = k.def;
      s.transitionS = k.transition;
      s.steps = k.steps;
      if (k.cls == Cls::Flip) {
        s.sw = Switch::EqualGainCrossfade;   // the manifest's switch {"method": "crossfade", "law": "equal-gain"}
        s.switchSource = "Weir manifest switch.method \"crossfade\"";
      }
      out.push_back(std::move(s));
    }
    return out;
  }
  std::string manifestJson() const override { return ""; }   // the rack reads the pinned file, never this
  Declared declared() const override { return {false, true, false}; }   // the manifest's changes_image, changes_level

  bool prepare(double sr, std::uint32_t maxBlock) override {
    if (!svf_) svf_ = std::make_unique<weir::Svf12<>>();
    svf_->prepare(sr, int(maxBlock), kSmoothingSeconds);
    dirty_ = true;
    return true;
  }
  // Weir's reset clears the state and makes the next setParams land at once: the values in hand land unramped.
  void reset() override {
    if (svf_) svf_->reset();
    dirty_ = true;
  }
  foundations::OperatorProperties properties() const override {
    foundations::OperatorProperties p;
    if (!svf_) return p;   // undeclared before prepare: validate() refuses it
    p.latency = foundations::LatencyDecl{0, 0.0, foundations::ResidualSign::kNone};
    p.tail = foundations::TailDecl{svf_->tail() < kRungOutThreshold ? 0u : foundations::kInfiniteTail, true};
    p.min_block = 1;
    return p;
  }

  int resolve(std::string_view address) const override {
    for (int k = 0; k < kCount; ++k)
      if (address == kKeys[k].address) return k;
    return -1;
  }
  bool setParam(int h, double v) override {
    if (h < 0 || h >= kCount || !std::isfinite(v)) return false;
    value_[h] = std::clamp(v, kKeys[h].min, kKeys[h].max);
    dirty_ = true;
    return true;
  }
  double getParam(int h) const override { return h >= 0 && h < kCount ? value_[h] : std::numeric_limits<double>::quiet_NaN(); }

  void process(const Sample* inL, const Sample* inR, Sample* outL, Sample* outR, std::uint32_t n) override {
    if (!svf_) return;
    if (dirty_) {
      weir::Params p;
      p.cutoffHz = value_[kCutoff];
      p.resonance = value_[kResonance];
      p.morph = value_[kMorph];   // Weir's morph knob is Ring (D-015)
      p.mode = 1 + int(std::lround(value_[kMode]));
      svf_->setParams(p);
      dirty_ = false;
    }
    std::copy_n(inL, n, outL);
    std::copy_n(inR, n, outR);
    svf_->process(outL, outR, int(n));
    const double mix = value_[kMix];
    for (std::uint32_t i = 0; i < n; ++i) {
      outL[i] = inL[i] * (1.0 - mix) + outL[i] * mix;
      outR[i] = inR[i] * (1.0 - mix) + outR[i] * mix;
    }
  }

  std::size_t stateBudget() const override { return kStateBudget; }
  std::size_t saveState(char* out, std::size_t cap) const override {
    std::size_t used = 0;
    for (int k = 0; k < kCount; ++k) {
      char line[96];
      const int len = std::snprintf(line, sizeof line, "%s %.17g\n", kKeys[k].address, value_[k]);
      if (len < 0 || used + std::size_t(len) > cap) return 0;
      std::memcpy(out + used, line, std::size_t(len));
      used += std::size_t(len);
    }
    return used;
  }
  bool stageState(std::string_view text) override {
    double v[kCount];
    for (int k = 0; k < kCount; ++k) v[k] = kKeys[k].def;
    std::istringstream in{std::string(text)};
    std::string address;
    double x = 0;
    while (in >> address >> x) {
      const int k = resolve(address);
      if (k < 0 || !std::isfinite(x)) return false;
      v[k] = std::clamp(x, kKeys[k].min, kKeys[k].max);
    }
    std::copy(v, v + kCount, staged_);
    return true;
  }
  bool adoptStaged() override {
    std::copy(staged_, staged_ + kCount, value_);
    dirty_ = true;
    return true;
  }

  bool parsePatch(std::string_view text, Patch& out, std::string* error) const override {
    std::istringstream in{std::string(text)};
    std::string address;
    double x = 0;
    out = Patch{};
    while (in >> address >> x) {
      if (resolve(address) < 0) {
        if (error) *error = "no key " + address;
        return false;
      }
      out.values.emplace_back(address, x);
    }
    return true;
  }
  std::string writePatch(const Patch& p) const override {
    std::string text;
    for (const auto& [address, v] : p.values) {
      char line[96];
      std::snprintf(line, sizeof line, "%s %.17g\n", address.c_str(), v);
      text += line;
    }
    return text;
  }
  bool stagePatch(const Patch& p) override {
    for (int k = 0; k < kCount; ++k) staged_[k] = kKeys[k].def;
    for (const auto& [address, v] : p.values) {
      const int k = resolve(address);
      if (k >= 0 && std::isfinite(v)) staged_[k] = std::clamp(v, kKeys[k].min, kKeys[k].max);
    }
    return true;
  }
  double macroValue(const Bind& b, double v) const override { return b.lo + (b.hi - b.lo) * std::clamp(v, 0.0, 1.0); }

  // Weir publishes no meters and keeps no guard of its own: the slot's are the module's (RS-4.7, RS-7.9).
  Meters meters() const override { return {}; }
  void clearMeters() override {}
  std::uint64_t nonFiniteCount() const override { return 0; }
  bool nonFiniteLatched() const override { return false; }
  void clearNonFiniteLatch() override {}
  bool plantNonFiniteState() override { return false; }   // Svf12's state is private: no seam

  Settings dryProbe() const override { return {{"fxfilter.mix", 0}}; }   // mix 0: the input exactly
  bool dryExact() const override { return true; }
  // Every stage engaged: a resonant high-pass with Ring, fully wet. The values decide what the slot's "values reach
  // the module" check exercises, so they are named for the tolerance inventory to list.
  static constexpr double kStressCutoffThresholdHz = 2000, kStressResonanceThreshold = 0.9, kStressRingThreshold = 0.5;
  Settings stressProbe() const override {
    return {{"fxfilter.mode", 2}, {"fxfilter.cutoff", kStressCutoffThresholdHz}, {"fxfilter.resonance", kStressResonanceThreshold},
            {"fxfilter.morph", kStressRingThreshold}, {"fxfilter.mix", 1}};
  }

 private:
  std::unique_ptr<weir::Svf12<>> svf_;
  double value_[kCount] = {}, staged_[kCount] = {};
  bool dirty_ = true;
};

}  // namespace

ModuleInfo weirModule(const std::string&) {
  ModuleInfo info;
  info.name = "Weir";
  info.make = [] { return std::make_unique<WeirModule>(); };
  return info;
}

}  // namespace admission
