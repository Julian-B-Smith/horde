/*
 * control_soak.h — the long-run soak's planted faults. ROADMAP B448 (Blind-Spot
 * Armor) phase 2, package P6, ruled in ADR-209; the proposal's row P6 asks for "a
 * planted slow drift, exaggerated so the control takes seconds", which "must go
 * red", and "a pre-aged-counter row".
 *
 * NOT engines, and never enrolled. tools/h2_soak_check.mjs plays the SAME note
 * stream through these adapters as through the real engine, on every run, and reads
 * them with the SAME judge. It is red unless each plant reads exactly as it is
 * built: the fault it carries, on the channel it is on, of the size its law gives,
 * and nothing else. A soak that passes these adapters cannot see the fault class
 * they carry.
 *
 * WHY ADAPTERS AND NOT THE ENGINE. A soak fault is slow by nature: the real thing
 * shows after hours. A plant must show in seconds, so it has to be built to a known
 * law. And the counter-wrap plants need a frame counter that STARTS just short of
 * the wrap, which no engine can be asked to do from outside (package P6 may not
 * edit the engine). For the real engine the wrap is reached only by rendering up
 * to it: the 192 kHz nightly cell, which passes 2^31 frames after 3.1 hours.
 *
 * ONE CLASS, SEVEN PLANTS. Every plant is the same small synthesiser: up to kVoices
 * sine voices, one per note-on, each with an envelope that rises toward 1 with time
 * constant kAttackS while its key is held and falls with kReleaseS after its
 * key-off, and is freed (exact zeros) below kFreeBelow. The two channels carry
 * kAmp x velocity x envelope per voice, times `gain`, through tanh, as the
 * engine's last line does, so the pre-limiter bus is read the same way. It takes
 * every key and reads only `gain`. Each note starts at phase 0, so a repeated
 * phrase renders the same samples each time round: it is as steady as a source
 * can be.
 *
 * THE FAULT IS ON THE RIGHT CHANNEL ONLY, in every plant. The left channel stays
 * healthy, so a judge that reads one channel misses every one of them. n counts
 * frames from prepare(), t = n / sample rate:
 *   control/soak-steady  no fault. The must-read-clean control: under the same note
 *                        churn it must show no drift, no wrap fault, nothing.
 *   control/soak-level   the right bus is multiplied by 2^(t / kLevelDoubleS): a
 *                        level that drifts up by 6.02 dB every kLevelDoubleS
 *                        seconds, for ever.
 *   control/soak-dc      while any voice sounds, kDcPerS x t x gain is added to the
 *                        right bus: an offset that creeps up, for ever.
 *   control/soak-wrap31  a frame counter that starts kAgeS seconds short of 2^31 and
 *                        is read as a SIGNED 32-bit number. From the frame it goes
 *                        negative the right bus is multiplied by kWrapStep: a level
 *                        step at the wrap, and the level stays there.
 *   control/soak-wrap32  a frame counter that starts kAgeS seconds short of 2^32 and
 *                        is kept in 32 bits. While any voice sounds, the right bus
 *                        carries a pilot tone of kPilotAmp x gain at kPilotHz whose
 *                        phase is computed from that counter. When the counter
 *                        returns to 0 the phase jumps: a click, with the same level
 *                        before and after.
 *   control/soak-nan     one NaN on the right output at t = kNanFromS and every
 *                        kNanEveryS after it, while any key is held: non-finite
 *                        late in the run, never in its first window.
 *   control/soak-hang    once a note has been played, whenever no key is held the
 *                        right bus carries kHangAmp x gain with its sign alternating
 *                        every frame, and never stops: a voice that did not free.
 *                        After the last key-off silence does not return.
 *
 * Every constant below is restated in tools/h2_soak_check.mjs (PLANTS), which works
 * out from them, in closed form, what each plant must read as. If either copy
 * changes alone the readings stop matching the predictions and the soak goes red:
 * the copies cannot rot quietly.
 *
 * kLevelCap. A level that really grew without bound would become infinity. The
 * growth stops at 1e30, hours beyond any run this file is used in.
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "harness.h"

namespace horde2::harness {

class SoakPlantControl final : public Adapter {
 public:
  enum Plant { kSteady, kLevel, kDc, kWrap31, kWrap32, kNan, kHang, kPlants };
  static constexpr int kVoices = 16;
  static constexpr double kAmp = 0.12;            // one voice at velocity 1 and gain 1, before tanh
  static constexpr double kAttackS = 0.004;       // the envelope's time constants, in seconds
  static constexpr double kReleaseS = 0.06;
  static constexpr double kFreeBelow = 1e-4;      // a released voice is freed under this envelope
  static constexpr double kLevelDoubleS = 235;    // soak-level: the right bus doubles every 235 s
  static constexpr double kDcPerS = 4e-5;         // soak-dc: offset added per second, at gain 1
  static constexpr double kAgeS = 51.3;           // the pre-aged counters start this long before their wrap
  static constexpr double kWrapStep = 1.26;       // soak-wrap31: the gain from the wrap on
  static constexpr double kPilotAmp = 0.1;        // soak-wrap32: the pilot tone, at gain 1 (about one voice)
  static constexpr double kPilotHz = 100;
  static constexpr double kNanFromS = 70.0;       // soak-nan: the first NaN, and the spacing after it
  static constexpr double kNanEveryS = 1.5;
  static constexpr double kHangAmp = 1e-3;        // soak-hang: the stuck level, -60 dBFS at gain 1
  static constexpr double kLevelCap = 1e30;

  explicit SoakPlantControl(Plant p) : plant(p) {}

  void prepare(double sampleRate, uint32_t) override {
    sr = sampleRate; gain = 0.35; n = 0; held = 0; played = false;
    for (Voice& v : voices) v = Voice{};
    aC = 1 - std::exp(-1 / (kAttackS * sr));
    rC = 1 - std::exp(-1 / (kReleaseS * sr));
    const uint64_t age = static_cast<uint64_t>(std::llround(kAgeS * sr));
    counter = plant == kWrap31 ? (1ull << 31) - age : plant == kWrap32 ? (1ull << 32) - age : 0;
    nanFrom = static_cast<uint64_t>(std::llround(kNanFromS * sr));
    nanEvery = static_cast<uint64_t>(std::llround(kNanEveryS * sr));
  }
  bool set(const char* key, double value) override {
    if (std::strcmp(key, "gain") == 0) gain = value;
    return true;
  }
  bool setString(const char*, const char*) override { return true; }
  void snap() override {}
  void noteOn(int note, double freq, double vel) override {
    // a free voice, else the quietest released one; with every voice held the note is dropped
    Voice* v = nullptr;
    for (Voice& x : voices) if (!x.active) { v = &x; break; }
    if (!v) for (Voice& x : voices) if (!x.gate && (!v || x.env < v->env)) v = &x;
    if (!v) return;
    *v = Voice{};
    v->active = true; v->gate = true; v->note = note; v->inc = freq / sr; v->vel = vel;
    held++; played = true;
  }
  void noteOff(int note) override {
    for (Voice& x : voices) if (x.active && x.gate && x.note == note) { x.gate = false; held--; }
  }
  void panic() override { for (Voice& x : voices) if (x.active && x.gate) { x.gate = false; held--; } }

  void render(double* L, double* R, int frames) override {
    for (int i = 0; i < frames; i++, n++, counter++) {
      double s = 0;
      bool sounding = false;
      for (Voice& v : voices) {
        if (!v.active) continue;
        if (v.gate) v.env += (1 - v.env) * aC;
        else { v.env -= v.env * rC; if (v.env < kFreeBelow) { v = Voice{}; continue; } }
        s += v.vel * v.env * std::sin(kTau * v.phase);
        v.phase += v.inc; v.phase -= std::floor(v.phase);
        sounding = true;
      }
      const double t = static_cast<double>(n) / sr;
      const double l = kAmp * s * gain;
      double r = l;
      if (plant == kLevel) { const double g = std::exp2(t / kLevelDoubleS); r *= g > kLevelCap ? kLevelCap : g; }
      if (plant == kDc && sounding) r += kDcPerS * t * gain;
      // the counter read as a signed 32-bit number: negative from 2^31 to 2^32
      if (plant == kWrap31 && static_cast<int32_t>(static_cast<uint32_t>(counter)) < 0) r *= kWrapStep;
      // the pilot's phase from the counter kept in 32 bits: it starts again from 0 at 2^32
      if (plant == kWrap32 && sounding) {
        const double ph = static_cast<double>(static_cast<uint32_t>(counter)) * kPilotHz / sr;
        r += kPilotAmp * gain * std::cos(kTau * (ph - std::floor(ph)));
      }
      if (plant == kHang && played && held == 0) r += ((n & 1u) ? -kHangAmp : kHangAmp) * gain;
      L[i] = std::tanh(l);
      R[i] = std::tanh(r);
      if (plant == kNan && held > 0 && n >= nanFrom && (n - nanFrom) % nanEvery == 0) R[i] = std::numeric_limits<double>::quiet_NaN();
    }
  }
  // The plants end in tanh, as the engine does, so the bus is read the same way.
  double preLimiter(double out) const override { return std::atanh(out); }

 private:
  static constexpr double kTau = 6.283185307179586;
  struct Voice { bool active = false, gate = false; int note = 0; double phase = 0, inc = 0, vel = 0, env = 0; };
  Plant plant;
  double sr = 48000, gain = 0.35, aC = 0, rC = 0;
  uint64_t n = 0, counter = 0, nanFrom = 0, nanEvery = 1;
  int held = 0;
  bool played = false;
  Voice voices[kVoices];
};

}  // namespace horde2::harness
