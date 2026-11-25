#pragma once
// The test-only post-engine reverb's DSP (B710, ADR-408): Scape's whole module (scape::FxModule, its FOUNDATIONS-shaped
// shim) between the engine's output (after B660's filter) and the host. Scape is a sibling repo, read only, built by its
// own core/CMakeLists.txt at the commit scape.pin names (CMakeLists.txt refuses any other), as tools/admission builds it.
//
// Every range, default, unit and class here is Scape's own (scape::paramInfo, scape::paramDecl), never a copy: what the
// host shows is what Scape declares at the pin. Scape ramps every value itself (its 20 ms ramps, the delay-length
// crossfade, Era's 20 ms switch), so the host's once-a-block steps need no smoothing here.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

#include "reverb_core.h"
#include "scape/fx_module.h"

namespace preview::reverb {

static_assert(kScapeParams == scape::kParamCount,
              "Scape's parameter count moved with its pin: re-lay reverb_core.h's host block (and re-test) in the same change");

inline const scape::ParamInfo& info(int k) { return scape::paramInfo(static_cast<scape::Param>(k)); }
inline const scape::ParamDecl& decl(int k) { return scape::paramDecl(static_cast<scape::Param>(k)); }
inline bool is_enum(int k) { return std::strcmp(decl(k).unit, "enum") == 0; }   // era, delayMotion: Scape rounds them
// Host values: Scape on is 0 or 1; every Scape parameter is in Scape's own units, on its own range.
inline double default_value(int k) { return k < 0 ? 0.0 : info(k).def; }
inline double min_value(int k) { return k < 0 ? 0.0 : info(k).min; }
inline double max_value(int k) { return k < 0 ? 1.0 : info(k).max; }
inline bool stepped(int k) { return k < 0 || is_enum(k); }
inline double clamped(int k, double v) {
  v = std::clamp(v, min_value(k), max_value(k));
  return stepped(k) ? std::round(v) : v;
}
// "Scape on", "Scape decay", "Scape inGain": Scape's own key names.
inline void param_name(int k, char* out, std::size_t size) {
  if (k < 0) std::snprintf(out, size, "Scape on");
  else std::snprintf(out, size, "Scape %s", info(k).name);
}
// The value in Scape's unit, or the enum's name from Scape's declared list ("clean|vintage").
inline void value_text(int k, double v, char* out, std::size_t size) {
  if (k < 0) { std::snprintf(out, size, "%s", v >= 0.5 ? "on" : "off"); return; }
  if (is_enum(k)) {
    const char* names = decl(k).values;
    int want = int(std::lround(v));
    while (want > 0 && std::strchr(names, '|')) { names = std::strchr(names, '|') + 1; --want; }
    const char* end = std::strchr(names, '|');
    std::snprintf(out, size, "%.*s", int(end ? end - names : std::ptrdiff_t(std::strlen(names))), names);
    return;
  }
  const char* unit = decl(k).unit;
  std::snprintf(out, size, *unit ? "%.3g %s" : "%.3g", std::round(v * 1e9) / 1e9 + 0.0, unit);
}

class PostReverb {
 public:
  static constexpr int kMaxBlock = 4096;   // the engine's own (DT-2)

  // Allocates (Scape holds 12 to 27 MiB per instance, its A1 figure): main thread, at activation.
  void prepare(double sample_rate) {
    if (!mod_) {
      mod_ = std::make_unique<scape::FxModule>();
      for (int k = 0; k < kScapeParams; ++k) handle_[std::size_t(k)] = *mod_->resolve(scape::FxModule::address(static_cast<scape::Param>(k)));
      for (auto& b : buf_) b = std::make_unique<double[]>(kMaxBlock);
    }
    mod_->prepare(sample_rate, kMaxBlock);
    fresh_ = true;
    quiet_ = kRungOut;
  }
  // The next block sends every value and clears Scape, so it lands at once and nothing stale rings out (audio thread).
  void reset() { fresh_ = true; }

  // In place, on the engine's float output, real-time safe after prepare. `v` is the kScapeParams host values in Scape's
  // order, read once per block. False when it left the buffers untouched: zeros in, and Scape's own declared tail
  // (FxModule::tail(), the bound in which every state reaches exact zero) has passed since the input last sounded, so its
  // output is exactly the zeros already there and an idle engine stays cheap (OU-4).
  bool process(const double* v, float* l, float* r, std::size_t frames) {
    if (!mod_) return false;
    if (fresh_) {
#ifndef HORDE_PREVIEW_PLANT_REVERB_DEAF   // smoke.cpp's must-fail build: the host's values never reach Scape
      for (int k = 0; k < kScapeParams; ++k) mod_->setParam(handle_[std::size_t(k)], v[k]);
#endif
      mod_->reset();   // after the values: reset snaps every one to its target (fx_module.h), so none ramps in
      for (int k = 0; k < kScapeParams; ++k) sent_[std::size_t(k)] = v[k];
      fresh_ = false;
      quiet_ = kRungOut;
    } else {
      for (int k = 0; k < kScapeParams; ++k) {
        if (v[k] == sent_[std::size_t(k)]) continue;
#ifndef HORDE_PREVIEW_PLANT_REVERB_DEAF
        mod_->setParam(handle_[std::size_t(k)], v[k]);   // Scape ramps it
#endif
        sent_[std::size_t(k)] = v[k];
      }
    }
    bool zeros = true;
    for (std::size_t i = 0; i < frames && zeros; ++i) zeros = l[i] == 0.0f && r[i] == 0.0f;
    if (zeros && quiet_ >= std::uint64_t(mod_->tail().samples)) return false;
    double* in_l = buf_[0].get();
    double* in_r = buf_[1].get();
    double* out_l = buf_[2].get();
    double* out_r = buf_[3].get();
    for (std::size_t done = 0; done < frames;) {
      const std::size_t n = std::min<std::size_t>(frames - done, kMaxBlock);
      for (std::size_t i = 0; i < n; ++i) { in_l[i] = l[done + i]; in_r[i] = r[done + i]; }
      mod_->process(in_l, in_r, out_l, out_r, std::uint32_t(n));
      for (std::size_t i = 0; i < n; ++i) { l[done + i] = float(out_l[i]); r[done + i] = float(out_r[i]); }
      done += n;
    }
    // Whole blocks only: a block with any sound restarts the count, which can only make the tail run longer than it must.
    quiet_ = zeros ? std::min(quiet_, kRungOut - frames) + frames : 0;
    return true;
  }

  const scape::FxModule* module() const { return mod_.get(); }   // tests: guard counters and read-outs

 private:
  static constexpr std::uint64_t kRungOut = std::numeric_limits<std::uint64_t>::max() / 2;   // nothing in it: cleared
  std::unique_ptr<scape::FxModule> mod_;
  std::array<scape::ParamHandle, kScapeParams> handle_{};
  std::array<double, kScapeParams> sent_{};
  std::array<std::unique_ptr<double[]>, 4> buf_;
  bool fresh_ = true;
  std::uint64_t quiet_ = kRungOut;   // samples of zero input since the input last sounded
};

}  // namespace preview::reverb
