/*
 * control_spread.h — the spread ramp's planted faults. ROADMAP B460 (ADR-216 D8).
 *
 * WHY THEY EXIST. tools/h2_spread_ramp_check.mjs judges that the member spread values
 * reach the blades as ramps (engine.h, rampSpread). A row that has never been seen to
 * fail proves nothing (LIBRARY L0032; control_steal.h is the precedent), so each fault
 * the check is there to catch has a plant here that the check must read as a fault, on
 * every run.
 *
 * NOT engines, and never enrolled. The check plays a job through one of these adapters,
 * reads it with the SAME measure as the real engine, and is red unless the plant reads
 * as the fault it carries.
 *
 *   id                    the fault
 *   control/ramp-blade1   THE RAMP REACHES BLADE 1 ONLY: blade 2 takes each tick's
 *                         values in one sample, as the present law does
 *   control/ramp-blade2   THE RAMP REACHES BLADE 2 ONLY: the same, the other way
 *   control/ramp-skip     A STEP LEFT AT THE TICK BOUNDARY: the ramp is dropped at one
 *                         tick in every kSkipEvery, so the values step there, and at
 *                         the tick after it too (the engine then starts its ramps
 *                         afresh, and a fresh start is a set)
 *   control/ramp-both     NOTHING PLANTED: the two-blade composition below with the
 *                         ramp on both blades. It must render what the engine renders,
 *                         or the two composed plants above mean nothing
 *
 * HOW, WITHOUT TOUCHING THE ENGINE. No engine can be asked from outside to ramp one
 * blade and not the other, and a second engine build would need a CMake target of
 * its own. Everything here goes through the shipped engine's public interface.
 *
 *   THE TWO-BLADE COMPOSITION (blade1, blade2, both). Three shipped engines get the
 *   same seed and the same calls, so their swarms and their spread laws are the same
 *   sample for sample:
 *     A  blade 2 made inert (its depth forced to 0: a blade of depth 0 returns its
 *        input, engine.h voicePlainK), so A's bus is the base wave plus blade 1's part;
 *     B  blade 1 made inert the same way: the base wave plus blade 2's part;
 *     C  both inert: the base wave alone.
 *   With the blades in parallel the member's output is blade 1 plus blade 2 less the
 *   base (engine.h outK), and everything after it up to the limiter is linear, so on
 *   the pre-limiter bus A + B - C is the whole engine. The plant gives A and B
 *   DIFFERENT `spreadRamp` values: blade1 leaves A's as the job set it and forces B's
 *   to 0; blade2 does the reverse; both forces nothing.
 *   WHAT THAT ASSUMES OF THE JOB, and what happens otherwise: the blades in parallel
 *   (b2mix 0), no Depth spread (dspread, dspread2 0: it would lift an inert blade's
 *   depth off 0), no cross-modulation or feedback, a base that is not the sine->saw
 *   shape, and NO NOISE BLADE (mode, mode2 not 3). A noise blade draws from the
 *   engine's one random stream at every noise step, and how many it takes depends on
 *   how its position moves; the drift law draws from the same stream, so a stepped
 *   engine and a ramped one would then walk different spreads. The check pins such a
 *   job and plays it through control/ramp-both as well: a job that breaks an
 *   assumption makes that render differ from the engine's, and the check is red.
 *
 *   THE SKIPPED TICK (skip). One shipped engine. The engine's couple tick falls on
 *   every kTick-th sample counted from its first (engine.h, kCoupleSamples and
 *   coupleTick), so the adapter counts samples, and at one tick in kSkipEvery it sets
 *   `spreadRamp` 0 for that one sample and puts the job's value back after it.
 *   Assumes every sample the engine renders goes through this adapter, which is so:
 *   prepare() builds the engine.
 *
 * The pre-limiter bus is the composed adapter's (atanh of the output).
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "../engine/engine.h"
#include "harness.h"

namespace horde2::harness {

class SpreadPlantControl final : public Adapter {
 public:
  enum Plant { kBlade1, kBlade2, kBoth, kSkip };
  // the engine's couple tick, in samples (engine.h kCoupleSamples, which is private)
  static constexpr int kTick = 32;
  // skip: the ramp is dropped at one tick in this many
  static constexpr int kSkipEvery = 4;

  explicit SpreadPlantControl(Plant p) : plant(p) {}
  void prepare(double sampleRate, uint32_t seed) override {
    for (int i = 0; i < kEngines; i++) {
      e[i].reset();
      if (i > 0 && plant == kSkip) continue;
      e[i] = std::make_unique<horde2::engine::Engine>(sampleRate);
      e[i]->seedRandom(seed);
    }
    pos = 0; ramp = 0;
    if (plant != kSkip) { e[kA]->set("depth2", 0); e[kB]->set("depth", 0); e[kC]->set("depth", 0); e[kC]->set("depth2", 0); }
  }
  bool set(const char* key, double value) override {
    if (plant == kSkip) { if (std::strcmp(key, "spreadRamp") == 0) ramp = value; return e[kA]->set(key, value); }
    bool known = true;
    for (int i = 0; i < kEngines; i++) {
      double v = value;
      // the inert blades stay inert whatever the job sets
      if (std::strcmp(key, "depth") == 0 && i != kA) v = 0;
      if (std::strcmp(key, "depth2") == 0 && i != kB) v = 0;
      // the plant itself: the engine carrying the unramped blade never hears the flag
      if (std::strcmp(key, "spreadRamp") == 0 && ((plant == kBlade1 && i == kB) || (plant == kBlade2 && i == kA))) v = 0;
      known = e[i]->set(key, v) && known;
    }
    return known;
  }
  bool setString(const char* key, const char* value) override { bool k = true; for (auto& x : e) if (x) k = x->setString(key, value) && k; return k; }
  void snap() override { for (auto& x : e) if (x) x->snap(); }
  void noteOn(int note, double freq, double vel) override { for (auto& x : e) if (x) x->noteOn(note, freq, vel); }
  void noteOff(int note) override { for (auto& x : e) if (x) x->noteOff(note); }
  void panic() override { for (auto& x : e) if (x) x->panic(); }
  void render(double* L, double* R, int n) override {
    if (plant == kSkip) {
      int done = 0;
      while (done < n) {
        const int at = static_cast<int>(pos % kTick);
        if (at == 0 && (pos / kTick) % kSkipEvery == 0) {   // this sample's tick is the dropped one
          e[kA]->set("spreadRamp", 0);
          e[kA]->render(L + done, R + done, 1);
          e[kA]->set("spreadRamp", ramp);
          done++; pos++;
          continue;
        }
        const int m = n - done < kTick - at ? n - done : kTick - at;   // up to the next tick
        e[kA]->render(L + done, R + done, m);
        done += m; pos += static_cast<uint64_t>(m);
      }
      return;
    }
    if (static_cast<int>(tl.size()) < n) { tl.resize(n); tr.resize(n); ul.resize(n); ur.resize(n); }
    e[kA]->render(L, R, n);
    e[kB]->render(tl.data(), tr.data(), n);
    e[kC]->render(ul.data(), ur.data(), n);
    for (int i = 0; i < n; i++) {
      L[i] = std::tanh(std::atanh(L[i]) + std::atanh(tl[i]) - std::atanh(ul[i]));
      R[i] = std::tanh(std::atanh(R[i]) + std::atanh(tr[i]) - std::atanh(ur[i]));
    }
  }
  double preLimiter(double out) const override { return std::atanh(out); }

 private:
  enum { kA, kB, kC, kEngines };
  const Plant plant;
  std::unique_ptr<horde2::engine::Engine> e[kEngines];   // skip: e[kA] alone
  std::vector<double> tl, tr, ul, ur;   // a control adapter: not a real-time path
  uint64_t pos = 0;    // skip: samples rendered since prepare()
  double ramp = 0;     // skip: the job's own spreadRamp
};

}  // namespace horde2::harness
