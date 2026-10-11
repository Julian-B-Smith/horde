/*
 * control_fuzz.h — the parameter fuzz's planted faults. ROADMAP B448 (Blind-Spot
 * Armor) phase 2, package P3, ruled in ADR-209; the proposal's row P3 asks for "a
 * corner-only instability and a NaN, planted behind fault flags", which "must be
 * found within the PR-size run".
 *
 * NOT engines, and never enrolled. tools/h2_fuzz_check.mjs runs the SAME cases over
 * these adapters as over the real engine, on every run, and is red unless it reads
 * each plant exactly as the plant is built: every case the plant's law says must
 * show, no other case, and the right KIND of reading. A fuzz that passes these
 * adapters cannot see the fault class they carry.
 *
 * WHY ADAPTERS AND NOT THE ENGINE'S OWN FAULT FLAGS. The render tool is built
 * without H2_ENGINE_FAULTS (adapter_composed.h: "the engine as it ships"), and a
 * second build of it is a CMake edit that package P3 was told not to make. A plant
 * in an adapter tests the same thing the fuzz must do: reach a corner of the
 * declared space and judge what comes out.
 *
 * ONE CLASS, TEN PLANTS. Each takes every key (so the engine's own cases run
 * unchanged) and watches only the keys of its corner, plus `gain`. A corner is
 * every watched key at a declared END of its range, or at one named option. The
 * corners are the rows of kCorners below, and nowhere else.
 *
 * WHAT EVERY PLANT RENDERS. While a note is held, both channels carry a level of
 * kAmp (about the level control/tone makes) whose sign alternates every frame, so
 * the peak of any stretch of frames is known exactly. All of it is multiplied by
 * the current `gain` and passed through tanh, as the engine's last line does: the
 * OUTPUT stays inside +-1 and only the pre-limiter bus shows a level fault (the
 * proposal: "tanh would hide a blow-up"), and the whole bus is linear in gain, which
 * is what lets the fuzz read it above the rail. With no note held the output is
 * exact zeros. At gain 0 a plant is silent, as an engine is.
 *
 * THE FAULT IS ON THE RIGHT CHANNEL ONLY, in every plant. The left channel keeps
 * the healthy level, so a judge that reads one channel misses every one of them.
 *
 * In its corner, while a note is held, n counting the frames of THIS visit:
 *   control/fuzz-blowup   kAmp doubling every kDoubleS (40 ms), without end: a
 *                         BLOW-UP that starts at a healthy level.
 *   control/fuzz-slow     kAmp doubling every kSlowDoubleS (400 ms): the same,
 *                         ten times slower. Only a long hold, probed for longer
 *                         in proportion, can decide it.
 *   control/fuzz-plateau  kAmp doubling every kDoubleS until it reaches
 *                         kPlateauLevel, then steady: a runaway that stops between
 *                         the fuzz's pass and fail lines. Once flat it is a level,
 *                         and the fuzz must call it one (grey), not a blow-up.
 *   control/fuzz-burst    kAmp, and for the last kBurstS of every kBurstEveryS it
 *                         is kBurstTimes larger: periodic bursts that never grow.
 *                         It must read CLEAN. Only a growth rule that needs every
 *                         one of its ratios to be large gets that right.
 *   control/fuzz-loud     kLoudLevel, far above the limiter's rail and never
 *                         changing: very loud and STEADY, not a blow-up.
 *   control/fuzz-keyed    kKeyedLevel, multiplied by kKeyTimes whenever level x
 *                         gain is above kKeyAbove: a stage before the limiter that
 *                         depends on the level. Its bus is NOT linear in gain, so a
 *                         reading through a divided gain is wrong, and the fuzz
 *                         must say so instead of reporting the number.
 *   control/fuzz-nan      one NaN on the first frame and on every kNanEvery-th.
 * And when the LAST held note is released in the corner (a second note may be
 * struck later; the release that found the corner is remembered):
 *   control/fuzz-nan-tail one NaN on the first frame with no note held and on
 *                         every kNanEvery-th such frame: non-finite only in the tail.
 *   control/fuzz-hang     a level of kHangAmp (-60 dBFS at gain 1) on every frame
 *                         with no note held, for ever: a voice that did not free.
 *   control/fuzz-late     the same for the first kLateS of such frames, then exact
 *                         zeros: a voice that frees late. It is silent at the end
 *                         of a render and not before it.
 *
 * The corner VALUES are the declared ends of tools/patchspace/dependency_tree.json
 * on 2026-10-11. Every constant below is restated in tools/h2_fuzz_check.mjs
 * (PLANTS), which works out from each case, in closed form, what this file must have
 * rendered. If a range shrinks so a corner cannot be reached, or either copy of a
 * constant changes alone, the fuzz stops matching the plant and goes red: the copies
 * cannot rot quietly.
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
  enum Plant { kBlowup, kSlow, kPlateau, kBurst, kLoud, kKeyed, kNan, kNanTail, kHang, kLate, kPlants };
  static constexpr double kAmp = 0.5;            // the healthy level at gain 1, before tanh
  static constexpr double kDoubleS = 0.04;       // the blow-up and the plateau double every 40 ms
  static constexpr double kSlowDoubleS = 0.4;    // the slow plant doubles every 400 ms
  static constexpr double kPlateauLevel = 64;    // where the plateau stops, at gain 1
  static constexpr double kBurstTimes = 12, kBurstEveryS = 0.35, kBurstS = 0.02;
  static constexpr double kLoudLevel = 4096;     // over 200 times the rail
  static constexpr double kKeyedLevel = 16, kKeyAbove = 10, kKeyTimes = 8;
  static constexpr double kHangAmp = 1e-3;       // the stuck level: -60 dBFS at gain 1
  static constexpr double kLateS = 0.65;         // the late voice frees after this long
  static constexpr double kLevelCap = 1e30;
  static constexpr long kNanEvery = 1024;

  // One watched key: at or above `at` ('>'), at or below it ('<'), or equal to it ('=').
  struct Watch { const char* key; char end; double at; };
  static constexpr int kWatches = 3;
  struct Corner { Watch w[kWatches]; };
  static const Corner& cornerOf(Plant p) {
    static const Corner kCorners[kPlants] = {
      /* blowup   */ {{{"fb", '>', 1}, {"K", '<', -1}, {nullptr, 0, 0}}},
      /* slow     */ {{{"stretchB", '>', 6}, {nullptr, 0, 0}, {nullptr, 0, 0}}},
      /* plateau  */ {{{"colB", '>', 1}, {"width", '<', 0}, {nullptr, 0, 0}}},
      /* burst    */ {{{"inertia", '>', 1}, {"harmReach", '<', 0.25}, {nullptr, 0, 0}}},
      /* loud     */ {{{"xm", '>', 1}, {"kRuleAmt", '<', 0}, {nullptr, 0, 0}}},
      /* keyed    */ {{{"spread", '>', 24}, {"onset", '<', -1}, {nullptr, 0, 0}}},
      /* nan      */ {{{"detune", '>', 100}, {"S", '<', 0}, {"aa", '=', 0}}},
      /* nan-tail */ {{{"hard", '>', 1}, {"depth", '<', 0}, {nullptr, 0, 0}}},
      /* hang     */ {{{"R", '<', 10}, {"A", '>', 2000}, {nullptr, 0, 0}}},
      /* late     */ {{{"R", '<', 10}, {"D", '>', 4000}, {nullptr, 0, 0}}},
    };
    return kCorners[p];
  }

  explicit FuzzPlantControl(Plant p) : plant(p) {}

  void prepare(double sampleRate, uint32_t) override {
    sr = sampleRate; frame = 0; cornerFrames = 0; released = false; sinceRelease = 0; held.clear();
    gain = 0.35;
    // No comparison with a NaN holds, so a key a case leaves unset is in no corner.
    for (double& v : val) v = std::numeric_limits<double>::quiet_NaN();
    burstEvery = std::lround(kBurstEveryS * sr); burstLen = std::lround(kBurstS * sr);
    lateFrames = std::lround(kLateS * sr);
  }
  bool set(const char* key, double value) override {
    if (std::strcmp(key, "gain") == 0) gain = value;
    const Corner& c = cornerOf(plant);
    for (int i = 0; i < kWatches; i++) if (c.w[i].key && std::strcmp(key, c.w[i].key) == 0) val[i] = value;
    return true;
  }
  bool setString(const char*, const char*) override { return true; }
  void snap() override {}
  void noteOn(int note, double, double) override { held.push_back(note); }
  void noteOff(int note) override {
    const bool was = !held.empty();
    for (size_t i = 0; i < held.size();) { if (held[i] == note) held.erase(held.begin() + static_cast<long>(i)); else i++; }
    // the release that finds the corner is remembered; a later one in the corner starts the count again
    if (was && held.empty() && inCorner()) { released = true; sinceRelease = 0; }
  }
  void panic() override { held.clear(); }

  void render(double* L, double* R_, int n) override {
    for (int i = 0; i < n; i++) {
      const bool on = !held.empty();
      const bool corner = on && inCorner();
      const double sign = (frame++ & 1u) ? -1.0 : 1.0;
      double l = 0, r = 0;   // the two levels at gain 1
      if (on) { l = kAmp; r = corner ? fault(cornerFrames) : kAmp; }
      else if (released && (plant == kHang || (plant == kLate && sinceRelease < lateFrames))) r = kHangAmp;
      double bus = r * gain;
      if (plant == kKeyed && bus > kKeyAbove) bus *= kKeyTimes;   // the one stage that is not linear in gain
      L[i] = std::tanh(sign * l * gain);
      R_[i] = std::tanh(sign * bus);
      if (plant == kNan && corner && cornerFrames % kNanEvery == 0) R_[i] = std::numeric_limits<double>::quiet_NaN();
      if (plant == kNanTail && !on && released && sinceRelease % kNanEvery == 0) R_[i] = std::numeric_limits<double>::quiet_NaN();
      // the count of frames in THIS visit to the corner: leaving it starts the count again
      cornerFrames = corner ? cornerFrames + 1 : 0;
      if (!on) sinceRelease++;
    }
  }
  // The plants end in tanh, as the engine does, so the bus is read the same way.
  double preLimiter(double out) const override { return std::atanh(out); }

 private:
  bool inCorner() const {
    const Corner& c = cornerOf(plant);
    for (int i = 0; i < kWatches; i++) {
      const Watch& w = c.w[i];
      if (!w.key) continue;
      if (w.end == '>' ? !(val[i] >= w.at) : w.end == '<' ? !(val[i] <= w.at) : !(val[i] == w.at)) return false;
    }
    return true;
  }
  // The right channel's level at gain 1 on corner frame n.
  double fault(long n) const {
    const double x = static_cast<double>(n);
    double level = kAmp;
    switch (plant) {
      case kBlowup: level = kAmp * std::exp2(x / (kDoubleS * sr)); break;
      case kSlow: level = kAmp * std::exp2(x / (kSlowDoubleS * sr)); break;
      case kPlateau: level = kAmp * std::exp2(x / (kDoubleS * sr)); if (level > kPlateauLevel) level = kPlateauLevel; break;
      case kBurst: if (n % burstEvery >= burstEvery - burstLen) level = kAmp * kBurstTimes; break;
      case kLoud: level = kLoudLevel; break;
      case kKeyed: level = kKeyedLevel; break;
      default: break;
    }
    return level > kLevelCap ? kLevelCap : level;
  }
  Plant plant;
  double sr = 48000, gain = 0.35;
  double val[kWatches] = {0, 0, 0};
  uint64_t frame = 0;
  long cornerFrames = 0, sinceRelease = 0, burstEvery = 1, burstLen = 0, lateFrames = 0;
  bool released = false;
  std::vector<int> held;
};

}  // namespace horde2::harness
