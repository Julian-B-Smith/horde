// plants.h — the admission test's must-fail controls (contract §2.2: "Every row has a must-fail
// control: a planted faulty version that must turn the row red, naming the module and the row").
//
// Each plant is a WRAPPER around a real module that injects one fault through the neutral ABI,
// so the same plant runs against every module's adapter and nothing is planted in a sibling's
// tree (tools/admission_check.py builds the sibling's sources unchanged). The driver runs every
// plant, judges whether its row went red with a failure the clean run did not have, and reports
// a row whose control did not fire as NOT PROVEN. A plant that cannot touch a module's internals
// models the fault at the boundary the slot sees: a reset that leaves a smoother unsnapped is a
// smoother the wrapper owns and does not snap, which the row's comparison cannot tell from one
// inside the module.
//
// THE LESSON THIS EXISTS FOR (horde LIBRARY, detector-shares-assumption): a probe here can
// confirm the expected answer for the wrong reason. A row is believed only after its own
// detector has been shown to read a planted fault.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "module.h"

namespace admission {

// A19's ruled per-corner figure (ADR-401), which the harness takes by --budget from
// tools/admission/state_budgets.json and sets here before any plant wraps a module. Read only by
// the three A19 plants below, which must land one byte past the figure of whatever module they
// wrap: a plant with its own hard-coded number would stop proving anything the day the figure moves.
// 0 until the harness sets it; tools/state_budget_check.py holds this default (and the harness's) at 0.
inline std::size_t ruledFigure = 0;

// Forwards everything; a plant overrides only the entry its fault lives in.
class Forward : public Module {
 public:
  explicit Forward(std::unique_ptr<Module> in) : in_(std::move(in)) {}
  std::vector<ParamSpec> manifest() const override { return in_->manifest(); }
  std::string manifestJson() const override { return in_->manifestJson(); }
  Declared declared() const override { return in_->declared(); }
  bool prepare(double sr, std::uint32_t mb) override { return in_->prepare(sr, mb); }
  void reset() override { in_->reset(); }
  foundations::OperatorProperties properties() const override { return in_->properties(); }
  int resolve(std::string_view a) const override { return in_->resolve(a); }
  bool setParam(int h, double v) override { return in_->setParam(h, v); }
  double getParam(int h) const override { return in_->getParam(h); }
  void process(const Sample* a, const Sample* b, Sample* c, Sample* d, std::uint32_t n) override { in_->process(a, b, c, d, n); }
  std::size_t stateBudget() const override { return in_->stateBudget(); }
  std::size_t saveState(char* o, std::size_t cap) const override { return in_->saveState(o, cap); }
  bool stageState(std::string_view t) override { return in_->stageState(t); }
  bool adoptStaged() override { return in_->adoptStaged(); }
  bool parsePatch(std::string_view t, Patch& o, std::string* e) const override { return in_->parsePatch(t, o, e); }
  std::string writePatch(const Patch& p) const override { return in_->writePatch(p); }
  bool stagePatch(const Patch& p) override { return in_->stagePatch(p); }
  double macroValue(const Bind& b, double v) const override { return in_->macroValue(b, v); }
  Meters meters() const override { return in_->meters(); }
  void clearMeters() override { in_->clearMeters(); }
  std::uint64_t nonFiniteCount() const override { return in_->nonFiniteCount(); }
  bool nonFiniteLatched() const override { return in_->nonFiniteLatched(); }
  void clearNonFiniteLatch() override { in_->clearNonFiniteLatch(); }
  bool plantNonFiniteState() override { return in_->plantNonFiniteState(); }
  Settings dryProbe() const override { return in_->dryProbe(); }
  bool dryExact() const override { return in_->dryExact(); }
  Settings stressProbe() const override { return in_->stressProbe(); }

 protected:
  // The first key of a class, by handle (-1 if none): where a plant puts a per-key fault.
  int firstOf(Cls c) const {
    for (const auto& s : in_->manifest())
      if (s.cls == c) return in_->resolve(s.address);
    return -1;
  }
  bool isClass(int h, Cls c) const {
    for (const auto& s : in_->manifest())
      if (s.cls == c && in_->resolve(s.address) == h) return true;
    return false;
  }
  bool isIo(int h) const { return h >= 0 && (h == in_->resolve("io.inGain") || h == in_->resolve("io.outGain")); }
  std::unique_ptr<Module> in_;
};

// A1: `tail` left undeclared after prepare (FOUNDATIONS' validate() must refuse it).
class TailUndeclared final : public Forward {
 public:
  using Forward::Forward;
  foundations::OperatorProperties properties() const override { auto p = in_->properties(); p.tail.reset(); return p; }
};

// A1: one parameter declared with no class.
class ClassMissing final : public Forward {
 public:
  using Forward::Forward;
  std::vector<ParamSpec> manifest() const override {
    auto m = in_->manifest();
    if (!m.empty()) m.front().cls = Cls::None;
    return m;
  }
};

// A2: a reset that leaves one smoother unsnapped. The wrapper owns a 20 ms linear smoother on
// the first morph key and multiplies the output by (1 + its distance from target): exactly 1 at
// rest, so it is inaudible to every row but one. reset() does not snap it.
class ResetUnsnapped final : public Forward {
 public:
  using Forward::Forward;
  bool prepare(double sr, std::uint32_t mb) override {
    key_ = firstOf(Cls::Morph);
    s_ = t_ = key_ >= 0 ? in_->getParam(key_) : 0;
    len_ = static_cast<long>(std::lround(0.02 * sr));
    left_ = 0;
    return in_->prepare(sr, mb);
  }
  bool setParam(int h, double v) override {
    const bool ok = in_->setParam(h, v);
    if (h == key_ && len_ > 0) { t_ = in_->getParam(h); inc_ = (t_ - s_) / static_cast<double>(len_); left_ = len_; }
    return ok;
  }
  void reset() override { in_->reset(); }   // THE FAULT: s_ keeps its place mid-ramp
  void process(const Sample* a, const Sample* b, Sample* oL, Sample* oR, std::uint32_t n) override {
    in_->process(a, b, oL, oR, n);
    for (std::uint32_t i = 0; i < n; ++i) {
      if (left_ > 0) { s_ = --left_ == 0 ? t_ : s_ + inc_; }
      oL[i] *= 1 + (s_ - t_);
      oR[i] *= 1 + (s_ - t_);
    }
  }

 private:
  int key_ = -1;
  double s_ = 0, t_ = 0, inc_ = 0;
  long len_ = 0, left_ = 0;
};

// A3: a ramp advanced per BLOCK, not per sample (Bulwark's rampPerBlock plant). The first morph
// key's target is fed to the module half the remaining way at each process() call, so how the
// stream is cut into blocks shapes what the module hears.
class RampPerBlock final : public Forward {
 public:
  using Forward::Forward;
  bool prepare(double sr, std::uint32_t mb) override {
    key_ = firstOf(Cls::Morph);
    cur_ = target_ = key_ >= 0 ? in_->getParam(key_) : 0;
    return in_->prepare(sr, mb);
  }
  bool setParam(int h, double v) override {
    if (h != key_) return in_->setParam(h, v);
    target_ = v;
    return true;
  }
  double getParam(int h) const override { return h == key_ ? target_ : in_->getParam(h); }
  void process(const Sample* a, const Sample* b, Sample* c, Sample* d, std::uint32_t n) override {
    if (key_ >= 0 && cur_ != target_) { cur_ += (target_ - cur_) / 2; in_->setParam(key_, cur_); }
    in_->process(a, b, c, d, n);
  }

 private:
  int key_ = -1;
  double cur_ = 0, target_ = 0;
};

// A4: latency declared one sample short — the output is delayed one sample more than declared.
class LatencyShort final : public Forward {
 public:
  using Forward::Forward;
  bool prepare(double sr, std::uint32_t mb) override { zL_ = zR_ = 0; return in_->prepare(sr, mb); }
  void reset() override { zL_ = zR_ = 0; in_->reset(); }
  void process(const Sample* a, const Sample* b, Sample* oL, Sample* oR, std::uint32_t n) override {
    in_->process(a, b, oL, oR, n);
    for (std::uint32_t i = 0; i < n; ++i) { std::swap(oL[i], zL_); std::swap(oR[i], zR_); }
  }

 private:
  Sample zL_ = 0, zR_ = 0;
};

// A4, the causality clause (ADR-351): an early leak 40 dB down. The main path is delayed kD more
// samples and declared kD later, so its peak still sits exactly at the declared latency and the
// impulse and sweep halves of the row stay clean; a copy at 1% (-40 dB) of the undelayed output
// leaks in at the module's true latency, kD samples before the declared one. kD = 4 puts it
// INSIDE the window of a module that declares an early residual (declared - 3, ADR-402), so the
// plant fires on every module, drive and half-band included (kD = 3 would land on the window's
// edge and read clean there). Only
// the leading-edge threshold (-60 dB re the peak) can see it, which is the point: a clause that
// asked for the first NONZERO sample would not tell this from a module with a quiet pre-ring.
class PreEcho final : public Forward {
 public:
  using Forward::Forward;
  static constexpr std::uint32_t kD = 4;
  bool prepare(double sr, std::uint32_t mb) override { clear(); return in_->prepare(sr, mb); }
  void reset() override { clear(); in_->reset(); }
  foundations::OperatorProperties properties() const override {
    auto p = in_->properties();
    if (p.latency) p.latency->samples += kD;
    if (p.tail && p.tail->samples != foundations::kInfiniteTail) p.tail->samples += kD;
    return p;
  }
  void process(const Sample* a, const Sample* b, Sample* oL, Sample* oR, std::uint32_t n) override {
    in_->process(a, b, oL, oR, n);
    for (std::uint32_t i = 0; i < n; ++i) {
      const Sample l = oL[i], r = oR[i];
      oL[i] = zL_[pos_] + 0.01 * l;   // a fixed ring: nothing allocates in process
      oR[i] = zR_[pos_] + 0.01 * r;
      zL_[pos_] = l;
      zR_[pos_] = r;
      pos_ = (pos_ + 1) % kD;
    }
  }

 private:
  void clear() { zL_.fill(0); zR_.fill(0); pos_ = 0; }
  std::array<Sample, kD> zL_{}, zR_{};
  std::uint32_t pos_ = 0;
};

// A5: a denormal residue. A leaky integrator of the output, added back at 1e-300 of its level:
// it decays into the subnormal range and then holds the smallest subnormal forever (0.9999 x
// that value rounds back to it), so the output never reaches exact zero. Silent from the start
// stays exact zero (the integrator starts empty), so only the tail half sees it.
class DenormalResidue final : public Forward {
 public:
  using Forward::Forward;
  bool prepare(double sr, std::uint32_t mb) override { y_ = 0; return in_->prepare(sr, mb); }
  void reset() override { y_ = 0; in_->reset(); }
  void process(const Sample* a, const Sample* b, Sample* oL, Sample* oR, std::uint32_t n) override {
    in_->process(a, b, oL, oR, n);
    for (std::uint32_t i = 0; i < n; ++i) {
      y_ = y_ * 0.9999 + oL[i] * 1e-300;
      oL[i] += y_;
      oR[i] += y_;
    }
  }

 private:
  double y_ = 0;
};

// A5: a tail declared a quarter of what the module rings for (Scape's own `tail` plant, in kind).
class TailShort final : public Forward {
 public:
  using Forward::Forward;
  foundations::OperatorProperties properties() const override {
    auto p = in_->properties();
    if (p.tail && p.tail->samples != foundations::kInfiniteTail) p.tail->samples /= 4;
    return p;
  }
};

// A5: a constant leaking into the output (Scape's own `silence` plant, in kind): silence in is
// no longer exact zero out, from the start or after a tail.
class SilenceLeak final : public Forward {
 public:
  using Forward::Forward;
  void process(const Sample* a, const Sample* b, Sample* oL, Sample* oR, std::uint32_t n) override {
    in_->process(a, b, oL, oR, n);
    for (std::uint32_t i = 0; i < n; ++i) { oL[i] += 1e-30; oR[i] += 1e-30; }
  }
};

// A6 test 1: the trims at 0 dB are not exactly unity (a law that maps 0 dB to 1e-9 dB).
class IoNotUnity final : public Forward {
 public:
  using Forward::Forward;
  bool setParam(int h, double v) override { return in_->setParam(h, isIo(h) ? v + 1e-9 : v); }
};

// A6 test 2: the trims jump. The wrapper keeps both gains itself, unramped, applied before and
// after the module, and holds the module's own pair at 0 dB.
class IoJump final : public Forward {
 public:
  using Forward::Forward;
  bool prepare(double sr, std::uint32_t mb) override {
    inH_ = in_->resolve("io.inGain");
    outH_ = in_->resolve("io.outGain");
    sL_.assign(mb, 0);
    sR_.assign(mb, 0);
    return in_->prepare(sr, mb);
  }
  bool setParam(int h, double v) override {
    if (h >= 0 && h == inH_) { gIn_ = std::pow(10, v / 20); dbIn_ = v; return in_->setParam(h, 0); }
    if (h >= 0 && h == outH_) { gOut_ = std::pow(10, v / 20); dbOut_ = v; return in_->setParam(h, 0); }
    return in_->setParam(h, v);
  }
  double getParam(int h) const override { return h == inH_ ? dbIn_ : h == outH_ ? dbOut_ : in_->getParam(h); }
  void process(const Sample* a, const Sample* b, Sample* oL, Sample* oR, std::uint32_t n) override {
    for (std::uint32_t i = 0; i < n; ++i) { sL_[i] = a[i] * gIn_; sR_[i] = b[i] * gIn_; }
    in_->process(sL_.data(), sR_.data(), oL, oR, n);
    for (std::uint32_t i = 0; i < n; ++i) { oL[i] *= gOut_; oR[i] *= gOut_; }
  }

 private:
  int inH_ = -1, outH_ = -1;
  double gIn_ = 1, gOut_ = 1, dbIn_ = 0, dbOut_ = 0;
  std::vector<Sample> sL_, sR_;
};

// A6 test 3: the trims open at 0 dB and ramp to the saved gain after the first block, instead
// of opening at it.
class IoOpensRamped final : public Forward {
 public:
  using Forward::Forward;
  bool prepare(double sr, std::uint32_t mb) override {
    const int hs[2] = {in_->resolve("io.inGain"), in_->resolve("io.outGain")};
    pending_.clear();
    for (int h : hs)
      if (h >= 0) { pending_.emplace_back(h, in_->getParam(h)); in_->setParam(h, 0); }
    return in_->prepare(sr, mb);
  }
  void process(const Sample* a, const Sample* b, Sample* c, Sample* d, std::uint32_t n) override {
    in_->process(a, b, c, d, n);
    for (const auto& [h, v] : pending_) in_->setParam(h, v);
    pending_.clear();
  }

 private:
  std::vector<std::pair<int, double>> pending_;
};

// A6 test 7: a clip latch that clears itself (the meters are cleared before every block).
class LatchSelfClears final : public Forward {
 public:
  using Forward::Forward;
  void process(const Sample* a, const Sample* b, Sample* c, Sample* d, std::uint32_t n) override {
    in_->clearMeters();
    in_->process(a, b, c, d, n);
  }
};

// A8: a mono-summing module that does not declare changes_image (the Notch case).
class MonoSum final : public Forward {
 public:
  using Forward::Forward;
  Declared declared() const override { auto d = in_->declared(); d.changesImage = false; return d; }
  void process(const Sample* a, const Sample* b, Sample* oL, Sample* oR, std::uint32_t n) override {
    in_->process(a, b, oL, oR, n);
    for (std::uint32_t i = 0; i < n; ++i) oL[i] = oR[i] = 0.5 * (oL[i] + oR[i]);
  }
};

// A9: a guard that zeroes without counting (the B448 B1 pattern): non-finite input is replaced
// by 0 before the module sees it, so the module's own counter never moves.
class GuardZeroUncounted final : public Forward {
 public:
  using Forward::Forward;
  bool prepare(double sr, std::uint32_t mb) override { sL_.assign(mb, 0); sR_.assign(mb, 0); return in_->prepare(sr, mb); }
  void process(const Sample* a, const Sample* b, Sample* c, Sample* d, std::uint32_t n) override {
    for (std::uint32_t i = 0; i < n; ++i) { sL_[i] = std::isfinite(a[i]) ? a[i] : 0; sR_[i] = std::isfinite(b[i]) ? b[i] : 0; }
    in_->process(sL_.data(), sR_.data(), c, d, n);
  }

 private:
  std::vector<Sample> sL_, sR_;
};

// A10: a malloc in process(). Through a volatile pointer: an allocation nothing reads back is
// elided by the optimiser and plants nothing (tools/rtsan_check.py, CALIBRATION).
class MallocInProcess final : public Forward {
 public:
  using Forward::Forward;
  void process(const Sample* a, const Sample* b, Sample* c, Sample* d, std::uint32_t n) override {
    static void* volatile sink;
    sink = std::malloc(64);
    std::free(sink);
    in_->process(a, b, c, d, n);
  }
};

// A11 (ADR-218): every flip-class key declared with its crossfade removed: a bare jump.
class FlipCrossfadeRemoved final : public Forward {
 public:
  using Forward::Forward;
  std::vector<ParamSpec> manifest() const override {
    auto m = in_->manifest();
    for (auto& s : m)
      if (s.cls == Cls::Flip) { s.sw = Switch::Jump; s.transitionS = 0; }
    return m;
  }
};

// A11: a flip-class parameter blended instead of flipped: the stepped keys declared morph-class,
// so the slot would blend values that the module can only step between.
class FlipBlended final : public Forward {
 public:
  using Forward::Forward;
  std::vector<ParamSpec> manifest() const override {
    auto m = in_->manifest();
    for (auto& s : m)
      if (s.cls == Cls::Flip) s.cls = Cls::Morph;
    return m;
  }
};

// A11: a Device key stored in a preset: written into every patch, and applied when one is loaded.
class DeviceInPreset final : public Forward {
 public:
  using Forward::Forward;
  std::string writePatch(const Patch& p) const override {
    std::string t = in_->writePatch(p);
    for (const auto& s : in_->manifest())
      if (s.cls == Cls::Device) {
        char b[64];
        std::snprintf(b, sizeof b, " %.17g\n", in_->getParam(in_->resolve(s.address)));
        t += s.address + b;
      }
    return t;
  }
  bool stagePatch(const Patch& p) override {
    for (const auto& [a, v] : p.values)
      if (isClass(in_->resolve(a), Cls::Device)) in_->setParam(in_->resolve(a), v);
    return in_->stagePatch(p);
  }
  bool parsePatch(std::string_view t, Patch& o, std::string* e) const override {
    if (!in_->parsePatch(t, o, e)) return false;
    for (const auto& s : in_->manifest()) {
      if (s.cls != Cls::Device) continue;
      const auto at = t.find(s.address + " ");
      if (at != std::string_view::npos) o.values.emplace_back(s.address, std::strtod(std::string(t.substr(at + s.address.size() + 1, 32)).c_str(), nullptr));
    }
    return true;
  }
};

// A11: a morph-class key that jumps: setParam lands it at once through the load entry, as a
// patch load would, instead of ramping under the module's smoothing.
class MorphLanded final : public Forward {
 public:
  using Forward::Forward;
  bool setParam(int h, double v) override {
    if (!isClass(h, Cls::Morph)) return in_->setParam(h, v);
    Patch p;
    p.name = "landed";
    for (const auto& s : in_->manifest()) {
      if (s.cls == Cls::Device) continue;
      const int k = in_->resolve(s.address);
      p.values.emplace_back(s.address, k == h ? v : in_->getParam(k));
    }
    return in_->stagePatch(p) && in_->adoptStaged();
  }
};

// A12: a load that reorders the macro slots.
class MacroReorder final : public Forward {
 public:
  using Forward::Forward;
  bool parsePatch(std::string_view t, Patch& o, std::string* e) const override {
    if (!in_->parsePatch(t, o, e)) return false;
    std::reverse(o.macros.begin(), o.macros.end());
    return true;
  }
};

// A15: detector state shared between instances (one static envelope for every instance of
// this plant) that leaks into the output. Two instances rendered side by side drive it twice.
class SharedDetector final : public Forward {
 public:
  using Forward::Forward;
  void process(const Sample* a, const Sample* b, Sample* oL, Sample* oR, std::uint32_t n) override {
    in_->process(a, b, oL, oR, n);
    for (std::uint32_t i = 0; i < n; ++i) {
      env() = env() * 0.999 + 0.001 * std::fabs(a[i]);
      oL[i] *= 1 + 1e-3 * env();
      oR[i] *= 1 + 1e-3 * env();
    }
  }

 private:
  static double& env() { static double e = 0; return e; }
};

// A17: a saved state missing one parameter (its last line is dropped).
class StateDropsParam final : public Forward {
 public:
  using Forward::Forward;
  std::size_t saveState(char* o, std::size_t cap) const override {
    std::size_t n = in_->saveState(o, cap);
    if (n < 2) return n;
    std::size_t cut = n - 1;   // the final '\n'
    while (cut > 0 && o[cut - 1] != '\n') --cut;
    return cut;
  }
};

// A18: every number in the saved state written through a lossy decimal format (%.6g, L0008).
// The state's lines are "<key> <number>"; the header line has no number and is kept.
class LossyFloat final : public Forward {
 public:
  using Forward::Forward;
  std::size_t saveState(char* o, std::size_t cap) const override {
    const std::size_t n = in_->saveState(o, cap);
    std::string src(o, n), out;
    std::size_t pos = 0;
    while (pos < src.size()) {
      std::size_t end = src.find('\n', pos);
      if (end == std::string::npos) end = src.size();
      std::string line = src.substr(pos, end - pos);
      const std::size_t sp = line.rfind(' ');
      char* stop = nullptr;
      const double v = sp == std::string::npos ? 0 : std::strtod(line.c_str() + sp + 1, &stop);
      if (sp != std::string::npos && stop && *stop == 0 && stop != line.c_str() + sp + 1) {
        char b[64];
        std::snprintf(b, sizeof b, "%.6g", v);
        line = line.substr(0, sp + 1) + b;
      }
      out += line + "\n";
      pos = end + 1;
    }
    if (out.size() > cap) return 0;
    std::memcpy(o, out.data(), out.size());
    return out.size();
  }
};

// A19: an oversized state copied (allocated) before it is judged, instead of refused first.
class OversizeCopied final : public Forward {
 public:
  using Forward::Forward;
  bool stageState(std::string_view t) override {
    const std::string copy(t);
    return in_->stageState(copy);
  }
};

// A19: a module that declares one byte more than its ruled figure (4,097 B against 4,096 B).
class BudgetOverFigure final : public Forward {
 public:
  using Forward::Forward;
  std::size_t stateBudget() const override { return ruledFigure + 1; }
};

// A19: a module that writes a factory preset back as patch text one byte over its ruled figure
// (ruling 1 counts the patch text). The padding need not parse: the row measures, it does not load.
class PatchTextOver final : public Forward {
 public:
  using Forward::Forward;
  std::string writePatch(const Patch& p) const override {
    std::string t = in_->writePatch(p);
    if (t.size() <= ruledFigure) t.append(ruledFigure + 1 - t.size(), ' ');
    return t;
  }
};

// A19: a factory preset SHIPPED one byte over the ruled figure. Planted on the preset list the
// harness reads, not on the module (a wrapper never sees a shipped file), so it proves the
// as-shipped text[] check, which patch-text-over (the written-back text) cannot reach. Padded with
// newlines; A19 does not load an over-figure text, so whether a parser would accept the padding
// does not matter. No presets = nothing planted: the control then reads as not fired, never a pass.
inline void presetTextOver(std::vector<std::pair<std::string, std::string>>& presets) {
  if (presets.empty()) return;
  std::string& t = presets.front().second;
  if (t.size() <= ruledFigure) t.append(ruledFigure + 1 - t.size(), '\n');
}

// A20: a meter latch serialised into the saved state.
class LatchInState final : public Forward {
 public:
  using Forward::Forward;
  std::size_t saveState(char* o, std::size_t cap) const override {
    std::size_t n = in_->saveState(o, cap);
    const Meters m = in_->meters();
    static constexpr char kLine[] = "meter.clip 1\n";
    if ((m.inClip || m.outClip) && n + sizeof kLine - 1 <= cap) { std::memcpy(o + n, kLine, sizeof kLine - 1); n += sizeof kLine - 1; }
    return n;
  }
};

// id -> (row, wrapper). The driver reads the list from `--list-plants`.
struct PlantDef {
  const char* id;
  const char* row;
  const char* what;
  std::unique_ptr<Module> (*wrap)(std::unique_ptr<Module>);
  // A plant on the shipped preset list rather than on the module (preset-text-over), or none.
  void (*presets)(std::vector<std::pair<std::string, std::string>>&) = nullptr;
};
template <class P>
std::unique_ptr<Module> wrapWith(std::unique_ptr<Module> m) { return std::make_unique<P>(std::move(m)); }

inline const std::vector<PlantDef>& plants() {
  static const std::vector<PlantDef> k = {
      {"tail-undeclared", "A1", "tail left undeclared after prepare", wrapWith<TailUndeclared>},
      {"class-missing", "A1", "a parameter missing its class", wrapWith<ClassMissing>},
      {"reset-unsnapped", "A2", "a reset that leaves one smoother unsnapped", wrapWith<ResetUnsnapped>},
      {"ramp-per-block", "A3", "ramps advanced per block, not per sample", wrapWith<RampPerBlock>},
      {"latency-short", "A4", "latency declared one sample short", wrapWith<LatencyShort>},
      {"pre-echo", "A4", "an early leak 40 dB down, before the declared latency", wrapWith<PreEcho>},
      {"denormal-residue", "A5", "a denormal residue in the tail", wrapWith<DenormalResidue>},
      {"tail-short", "A5", "a tail longer than declared", wrapWith<TailShort>},
      {"silence-leak", "A5", "a constant leaking into silence", wrapWith<SilenceLeak>},
      {"io-not-unity", "A6", "test 1: the trims at 0 dB are not exactly unity", wrapWith<IoNotUnity>},
      {"io-jump", "A6", "test 2: the trims jump instead of ramping", wrapWith<IoJump>},
      {"io-opens-ramped", "A6", "test 3: the trims open at 0 dB, not at the saved gain", wrapWith<IoOpensRamped>},
      {"latch-self-clears", "A6", "test 7: a clip latch that clears itself", wrapWith<LatchSelfClears>},
      {"mono-sum", "A8", "a mono-summing module that does not declare changes_image", wrapWith<MonoSum>},
      {"guard-zero-uncounted", "A9", "a guard that zeroes without counting", wrapWith<GuardZeroUncounted>},
      {"malloc-in-process", "A10", "a malloc in process", wrapWith<MallocInProcess>},
      {"flip-crossfade-removed", "A11", "a flip-class key switched with its crossfade removed", wrapWith<FlipCrossfadeRemoved>},
      {"flip-blended", "A11", "a flip-class parameter blended instead of flipped", wrapWith<FlipBlended>},
      {"device-in-preset", "A11", "a Device key stored in a preset", wrapWith<DeviceInPreset>},
      {"morph-landed", "A11", "a morph-class key that jumps instead of ramping", wrapWith<MorphLanded>},
      {"macro-reorder", "A12", "a load that reorders slots", wrapWith<MacroReorder>},
      {"shared-detector", "A15", "shared detector state between instances", wrapWith<SharedDetector>},
      {"state-drops-param", "A17", "one parameter missing from the saved state", wrapWith<StateDropsParam>},
      {"lossy-float", "A18", "a float written through a lossy decimal format", wrapWith<LossyFloat>},
      {"oversize-copied", "A19", "an oversized state copied before it is refused", wrapWith<OversizeCopied>},
      {"budget-over-figure", "A19", "a module declaring its ruled figure + 1 byte", wrapWith<BudgetOverFigure>},
      {"patch-text-over", "A19", "a preset written back one byte over the ruled figure", wrapWith<PatchTextOver>},
      {"preset-text-over", "A19", "a factory preset shipped one byte over the ruled figure", wrapWith<Forward>, presetTextOver},
      {"latch-in-state", "A20", "a meter latch serialised into the state", wrapWith<LatchInState>},
  };
  return k;
}

}  // namespace admission
