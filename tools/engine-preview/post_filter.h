#pragma once
// The test-only post-engine filter's DSP (B660): one stereo Weir SVF 12 between the engine's output and the host.
// Weir is a sibling repo, read only, linked at the commit weir.pin names (CMakeLists.txt refuses any other).
//
// Weir's own ramp is ON here (kSmoothingSeconds), unlike horde's engine, which calls prepare(sr, 4096, 0.0) because its
// PA-1 smoother already glides every knob (ADR-311). Nothing glides these knobs: the host steps them once per block,
// so without Weir's ramp a sweep would zipper at the block rate.
#include <algorithm>
#include <cstddef>
#include <memory>

#include "filter_core.h"
#include "weir/svf.h"

namespace preview::filter {

class PostFilter {
 public:
  static constexpr int kMaxBlock = 4096;   // Weir's maxBlock, the same as the engine's (DT-2)
  static constexpr double kSmoothingSeconds = 0.005;

  // Allocates: main thread, at activation.
  void prepare(double sample_rate) {
    if (!svf_) {
      svf_ = std::make_unique<weir::Svf12<>>();
      left_ = std::make_unique<double[]>(kMaxBlock);
      right_ = std::make_unique<double[]>(kMaxBlock);
    }
    svf_->prepare(sample_rate, kMaxBlock, kSmoothingSeconds);
    fresh_ = true;
  }
  // The next block's first setParams lands at once instead of ramping from a stale value.
  void reset() { if (svf_) svf_->reset(); fresh_ = true; }

  // Weir's own bound on what the state can still produce with no input (tail(), its RC-1 tail report): under 1e-7 is
  // inaudible, below -140 dB, and an idle engine's skip needs no filter pass.
  bool rung_out() const { return !svf_ || svf_->tail() < 1e-7; }

  // In place, on the engine's float output; real-time safe after prepare. `s` is the host's values, read once per block.
  void process(const State& s, float* l, float* r, std::size_t frames) {
    if (!svf_) return;
    if (fresh_) { svf_->reset(); fresh_ = false; }
    weir::Params p;
    p.cutoffHz = cutoff_hz(s.v[kCutoff]);
    p.resonance = s.v[kResonance];
    p.morph = s.v[kRing];   // Weir's morph knob is Ring (D-015)
    p.mode = 1 + int(s.v[kMode]);
    svf_->setParams(p);
    const double mix = s.v[kMix];
    for (std::size_t done = 0; done < frames;) {
      const int n = int(std::min<std::size_t>(frames - done, kMaxBlock));
      for (int i = 0; i < n; ++i) { left_[std::size_t(i)] = l[done + std::size_t(i)]; right_[std::size_t(i)] = r[done + std::size_t(i)]; }
      svf_->process(left_.get(), right_.get(), n);
      for (int i = 0; i < n; ++i) {
        const std::size_t k = done + std::size_t(i);
        l[k] = float(double(l[k]) * (1.0 - mix) + left_[std::size_t(i)] * mix);
        r[k] = float(double(r[k]) * (1.0 - mix) + right_[std::size_t(i)] * mix);
      }
      done += std::size_t(n);
    }
  }

 private:
  std::unique_ptr<weir::Svf12<>> svf_;
  std::unique_ptr<double[]> left_, right_;
  bool fresh_ = true;
};

}  // namespace preview::filter
