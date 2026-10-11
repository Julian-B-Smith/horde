/*
 * control_fuzz.h — the parameter fuzz's planted faults. ROADMAP B448 (Blind-Spot
 * Armor) phase 2, package P3, ruled in ADR-209; the proposal's row P3 asks for "a
 * corner-only instability and a NaN, planted behind fault flags", which "must be
 * found within the PR-size run".
 *
 * NOT engines, and never enrolled. tools/h2_fuzz_check.mjs runs the SAME cases over
 * these adapters as over the real engine, on every run, and is red unless it reads
 * each plant exactly as the plant is built: every case that enters the corner, no
 * other case, and the right KIND of fault. A fuzz that passes these adapters cannot
 * see the fault class they carry.
 *
 * WHY ADAPTERS AND NOT THE ENGINE'S OWN FAULT FLAGS. The render tool is built
 * without H2_ENGINE_FAULTS (adapter_composed.h: "the engine as it ships"), and a
 * second build of it is a CMake edit that package P3 was told not to make. A plant
 * in an adapter tests the same thing the fuzz must do: reach a corner of the
 * declared space and judge what comes out.
 *
 * ONE CLASS, FOUR PLANTS. Each takes every key (so the engine's own cases run
 * unchanged) and watches only the keys of its corner, plus `gain` where its fault
 * is a level. A corner is every watched key at a declared END of its range, or at
 * one named option, WHILE A NOTE IS HELD:
 *
 *   control/fuzz-blowup   fb at its maximum AND K at its minimum. A level that
 *                         starts at kStart * gain and DOUBLES every kDoubleS
 *                         seconds for as long as the corner is held: growing
 *                         without bound. It must read as a BLOW-UP.
 *   control/fuzz-loud     xm at its maximum AND kRuleAmt at its minimum. A level
 *                         of exactly kLoud * gain, far above the limiter's rail,
 *                         that never changes: very loud and STEADY. It must read
 *                         as a level, at that number, and NOT as a blow-up.
 *   control/fuzz-nan      detune at its maximum AND S at its minimum AND aa = 0.
 *                         One NaN on the RIGHT channel on the first frame rendered
 *                         there, and on every 1024th after it. A judge that reads
 *                         one channel, or a block statistic, misses it.
 *   control/fuzz-hang     R at its minimum AND A at its maximum WHEN the last held
 *                         note is released. From then on a tone at kHangAmp (-60
 *                         dBFS) that never stops: a voice that did not free.
 *
 * THE TWO LEVEL PLANTS FOLLOW `gain` AS THE ENGINE DOES. Their bus is the fault's
 * level times the current gain, then tanh, so the OUTPUT stays inside +-1 and only
 * the pre-limiter bus shows the fault (the proposal: "tanh would hide a blow-up").
 * That is what lets the fuzz read them above the rail: it replays a case with gain
 * divided down and scales the reading back. Their level is the same on every frame
 * (the sign alternates), so the peak of any stretch is known exactly, and the fuzz
 * checks the number it reads against the number planted here. With gain at 0 a
 * level plant is silent, as an engine is. Their healthy tone follows gain as well:
 * a part of the bus that did not would be multiplied back as if it had, and read
 * as a level of a quarter of the dividing factor.
 *
 * The corner VALUES below are the declared ends of tools/patchspace/dependency_tree.json
 * on 2026-10-11 (fb 0..1, K -1..1, xm 0..1, kRuleAmt 0..1, detune 0..100, S 0..1,
 * R 10..5000, A 1..2000), compared with >= and <=. kStart, kDoubleS and kLoud are
 * restated in tools/h2_fuzz_check.mjs (PLANTS), which works out from each case what
 * this file must have rendered. If a range shrinks so a corner cannot be reached, or
 * either copy of a constant changes alone, the fuzz stops matching the plant and its
 * check goes red: the copies cannot rot quietly.
 *
 * Outside its corner each plant is a quiet sine through tanh while a note is held
 * (kAmp, times gain for the two level plants), and exact zeros after the release.
 *
 * kLevelCap. A level that really grew without bound would become infinity, and an
 * infinite level is one step from a NaN: the NaN plant's fault, not the blow-up's.
 * The growth therefore stops at 1e30, far beyond anything the fuzz can read.
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
  enum Plant { kBlowup, kLoud, kNan, kHang };
  static constexpr double kAmp = 0.25;        // the healthy level, before tanh
  static constexpr double kStart = 4096;      // the blow-up's level on its first frame, at gain 1
  static constexpr double kDoubleS = 0.04;    // the blow-up doubles every 40 ms
  static constexpr double kLoudLevel = 4096;  // the steady plant's level at gain 1: over 200 times the rail
  static constexpr double kHangAmp = 1e-3;    // the stuck tone: -60 dBFS
  static constexpr double kLevelCap = 1e30;
  static constexpr int kNanEvery = 1024;

  explicit FuzzPlantControl(Plant p) : plant(p) {}

  void prepare(double sampleRate, uint32_t) override {
    sr = sampleRate; phase = 0; cornerFrames = 0; hung = false; held.clear();
    // a state that is in no corner, whatever a case leaves unset
    fb = 0; K = 0; xm = 0; kRuleAmt = 1; detune = 0; S = 1; aa = 1; R = 280; A = 4; gain = 0.35;
  }
  bool set(const char* key, double value) override {
    if (std::strcmp(key, "fb") == 0) fb = value;
    else if (std::strcmp(key, "K") == 0) K = value;
    else if (std::strcmp(key, "xm") == 0) xm = value;
    else if (std::strcmp(key, "kRuleAmt") == 0) kRuleAmt = value;
    else if (std::strcmp(key, "detune") == 0) detune = value;
    else if (std::strcmp(key, "S") == 0) S = value;
    else if (std::strcmp(key, "aa") == 0) aa = value;
    else if (std::strcmp(key, "R") == 0) R = value;
    else if (std::strcmp(key, "A") == 0) A = value;
    else if (std::strcmp(key, "gain") == 0) gain = value;
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
      double y = 0;
      if (corner && (plant == kBlowup || plant == kLoud)) {
        // a level plant: the same size on every frame, the sign alternating
        double level = kLoudLevel;
        if (plant == kBlowup) {
          level = kStart * std::exp2(static_cast<double>(cornerFrames) / (kDoubleS * sr));
          if (level > kLevelCap) level = kLevelCap;
        }
        y = std::tanh((cornerFrames % 2 ? -1.0 : 1.0) * level * gain);
      }
      // out of the corner a level plant's healthy tone follows gain too, so its WHOLE bus is linear in gain
      else if (on) y = std::tanh(kAmp * (plant == kBlowup || plant == kLoud ? gain : 1.0) * std::sin(kTau * phase));
      else if (hung) y = std::tanh(kHangAmp * std::sin(kTau * phase));
      L[i] = R_[i] = y;
      if (plant == kNan && corner && cornerFrames % kNanEvery == 0) R_[i] = std::numeric_limits<double>::quiet_NaN();
      // the count of frames in THIS visit to the corner: leaving it starts the count again
      cornerFrames = corner ? cornerFrames + 1 : 0;
      if (on) freq = held.back().freq;
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
      case kLoud: return xm >= 1 && kRuleAmt <= 0;
      case kNan: return detune >= 100 && S <= 0 && aa == 0;
      case kHang: return R <= 10 && A >= 2000;
    }
    return false;
  }
  static constexpr double kTau = 6.283185307179586;
  struct Held { int note; double freq; };
  Plant plant;
  double sr = 48000, phase = 0, freq = 440;
  double fb = 0, K = 0, xm = 0, kRuleAmt = 1, detune = 0, S = 1, aa = 1, R = 280, A = 4, gain = 0.35;
  long cornerFrames = 0;
  bool hung = false;
  std::vector<Held> held;
};

}  // namespace horde2::harness
