/*
 * control_fuzz.h — the parameter fuzz's planted faults. ROADMAP B448 (Blind-Spot
 * Armor) phase 2, package P3, ruled in ADR-209; the proposal's row P3 asks for "a
 * corner-only instability and a NaN, planted behind fault flags", which "must be
 * found within the PR-size run".
 *
 * NOT engines, and never enrolled. tools/h2_fuzz_check.mjs runs the SAME cases over
 * these adapters as over the real engine, on every run, and is red unless it finds
 * each plant, names its corner and reads nothing outside it. A fuzz that passes
 * these adapters cannot see the fault class they carry.
 *
 * WHY ADAPTERS AND NOT THE ENGINE'S OWN FAULT FLAGS. The render tool is built
 * without H2_ENGINE_FAULTS (adapter_composed.h: "the engine as it ships"), and a
 * second build of it is a CMake edit that package P3 was told not to make. A plant
 * in an adapter tests the same thing the fuzz must do: reach a corner of the
 * declared space and judge what comes out.
 *
 * ONE CLASS, THREE PLANTS. Each takes every key (so the engine's own cases run
 * unchanged) and watches only the keys of its corner. A corner is every watched
 * key at a declared END of its range, or at one named option:
 *
 *   control/fuzz-blowup   fb at its maximum AND K at its minimum. While a note is
 *                         held there, the level doubles every 16 frames; out of the
 *                         corner it halves back at the same rate. The output goes
 *                         through tanh, as the engine's does, so the OUTPUT stays
 *                         inside +-1 and only the pre-limiter bus shows the fault:
 *                         the case the proposal names ("tanh would hide a blow-up").
 *   control/fuzz-nan      detune at its maximum AND S at its minimum AND aa = 0.
 *                         One NaN on the RIGHT channel on the first frame rendered
 *                         there with a note held, and on every 1024th after it. A
 *                         judge that reads one channel, or a block statistic, misses it.
 *   control/fuzz-hang     R at its minimum AND A at its maximum WHEN the last held
 *                         note is released. From then on a tone at kHangAmp (-60
 *                         dBFS) that never stops: a voice that did not free.
 *
 * The corner VALUES below are the declared ends of tools/patchspace/dependency_tree.json
 * on 2026-10-11 (fb 0..1, K -1..1, detune 0..100, S 0..1, R 10..5000, A 1..2000),
 * compared with >= and <=. If a range shrinks so a corner cannot be reached, the fuzz
 * stops finding the plant and its check goes red: the copy cannot rot quietly.
 *
 * Outside its corner each plant is a quiet sine through tanh while a note is held,
 * and exact zeros after the release. So the fuzz's verdict on it is two-sided: every
 * case that enters the corner must fail, and no other case may.
 *
 * kLevelCap. A level that really grew without bound would become infinity, and an
 * infinite level times a zero crossing is a NaN: the NaN plant's fault, not this
 * one's. The growth therefore stops at 1e30, which is as far above the limiter's
 * rail (about 19, adapter_composed.h) as a double needs to read "blown up".
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "harness.h"

namespace horde2::harness {

class FuzzPlantControl final : public Adapter {
 public:
  enum Plant { kBlowup, kNan, kHang };
  static constexpr double kAmp = 0.25;        // the healthy level, before tanh
  static constexpr double kHangAmp = 1e-3;    // the stuck tone: -60 dBFS
  static constexpr double kLevelCap = 1e30;
  static constexpr int kNanEvery = 1024;

  explicit FuzzPlantControl(Plant p) : plant(p) {}

  void prepare(double sampleRate, uint32_t) override {
    sr = sampleRate; phase = 0; level = 1; cornerFrames = 0; hung = false; held.clear();
    // a state that is in no corner, whatever a case leaves unset
    fb = 0; K = 0; detune = 0; S = 1; aa = 1; R = 280; A = 4;
  }
  bool set(const char* key, double value) override {
    if (std::strcmp(key, "fb") == 0) fb = value;
    else if (std::strcmp(key, "K") == 0) K = value;
    else if (std::strcmp(key, "detune") == 0) detune = value;
    else if (std::strcmp(key, "S") == 0) S = value;
    else if (std::strcmp(key, "aa") == 0) aa = value;
    else if (std::strcmp(key, "R") == 0) R = value;
    else if (std::strcmp(key, "A") == 0) A = value;
    return true;
  }
  bool setString(const char*, const char*) override { return true; }
  void snap() override {}
  void noteOn(int note, double freq, double) override { held.push_back({note, freq}); }
  void noteOff(int note) override {
    const bool was = !held.empty();
    for (size_t i = 0; i < held.size();) { if (held[i].note == note) held.erase(held.begin() + static_cast<long>(i)); else i++; }
    if (plant == kHang && was && held.empty() && inCorner()) hung = true;
  }
  void panic() override { held.clear(); }

  void render(double* L, double* R_, int n) override {
    for (int i = 0; i < n; i++) {
      const bool on = !held.empty();
      const bool corner = on && inCorner();
      if (plant == kBlowup) {
        level *= corner ? kDouble16 : 1 / kDouble16;
        if (level > kLevelCap) level = kLevelCap;
        if (level < 1) level = 1;
      }
      double y = 0;
      if (on) y = std::tanh(kAmp * level * std::sin(kTau * phase));
      else if (hung) y = std::tanh(kHangAmp * std::sin(kTau * phase));
      L[i] = R_[i] = y;
      if (plant == kNan && corner && cornerFrames++ % kNanEvery == 0) R_[i] = std::numeric_limits<double>::quiet_NaN();
      if (on) { freq = held.back().freq; }
      phase += freq / sr;
      phase -= std::floor(phase);
    }
  }
  // The plants end in tanh, as the engine does, so the bus is read the same way.
  double preLimiter(double out) const override { return std::atanh(out); }

 private:
  bool inCorner() const {
    switch (plant) {
      case kBlowup: return fb >= 1 && K <= -1;
      case kNan: return detune >= 100 && S <= 0 && aa == 0;
      case kHang: return R <= 10 && A >= 2000;
    }
    return false;
  }
  static constexpr double kTau = 6.283185307179586;
  static constexpr double kDouble16 = 1.0442737824274138;   // 2^(1/16): the level doubles every 16 frames
  struct Held { int note; double freq; };
  Plant plant;
  double sr = 48000, phase = 0, freq = 440, level = 1;
  double fb = 0, K = 0, detune = 0, S = 1, aa = 1, R = 280, A = 4;
  long cornerFrames = 0;
  bool hung = false;
  std::vector<Held> held;
};

}  // namespace horde2::harness
